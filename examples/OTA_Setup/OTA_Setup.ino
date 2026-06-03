/**
 * FA-LECTRIC IoT — OTA Setup Firmware (Bootstrap + Dynamic VM)
 *
 * Program awal yang di-flash via kabel (website fa-lectric.com/iot/flash).
 * Tugasnya:
 *   1. Terima konfigurasi WiFi + API Key via Serial (dari website saat flash).
 *   2. Simpan ke NVS (memori permanen).
 *   3. Connect WiFi + WebSocket ke server.
 *   4. Dengarkan perintah OTA dari aplikasi -> unduh & flash program baru
 *      (mis. BasicRealtimeDB, RelayControl, dll) TANPA kabel lagi.
 *   5. [BARU] Terima program JSON Bytecode dari Web Console dan eksekusi
 *      secara langsung tanpa upload ulang (Zero-OTA Dynamic VM).
 *
 * Board: ESP32 Dev Module (atau varian lain)
 * Partition Scheme: Default 4MB with spiffs
 */

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

Preferences prefs;
WebSocketsClient ws;

String storedSSID     = "";
String storedPass     = "";
String storedKey      = "";
String storedServer   = "";
String storedDeviceId = "";
uint16_t storedWsPort = 443;

bool wsConnected      = false;
unsigned long lastReconnect  = 0;
unsigned long lastHeartbeat  = 0;

// ---- Zero-OTA Dynamic VM state ----
struct VMVar { char name[24]; float value; };
struct VMProgram {
  bool loaded    = false;
  bool running   = false;
  bool setupDone = false;
  String setupJson;
  String loopJson;
  VMVar vars[20];
  uint8_t varCount = 0;
} vm;

// Forward declarations VM
void vmLoadProgram(const char* setupJson, const char* loopJson, const char* varsJson);
void vmRunBlock(const char* jsonArray);
void vmExecInstr(JsonArray& instr);
void vmSetVar(const char* name, float val);
float vmGetVar(const char* name);
void vmSendLog(const char* msg);
bool vmEvalCondition(const char* leftName, const char* op, float rightVal);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("[FA] ============================");
  Serial.println("[FA] FA-LECTRIC IoT OTA Setup v2.1.0");
  Serial.println("[FA] ============================");

  prefs.begin("fa-iot", false);
  storedSSID = prefs.getString("ssid", "");
  storedPass = prefs.getString("pass", "");
  storedKey = prefs.getString("key", "");
  storedServer = prefs.getString("server", "fa-lectric.com");
  storedDeviceId = prefs.getString("deviceId", "");
  storedWsPort = prefs.getUShort("wsport", 443);
  prefs.end();

  if (storedSSID.length() > 0 && storedKey.length() > 0) {
    Serial.println("[FA] Konfigurasi ditemukan di memori.");
    Serial.println("[FA] SSID: " + storedSSID);
    connectWiFi();
  } else {
    Serial.println("[FA] Belum ada konfigurasi.");
    Serial.println("[FA] Menunggu konfigurasi dari Serial...");
  }
}

void loop() {
  // Cek Serial untuk konfigurasi baru (dari website saat flash)
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.startsWith("{")) {
      handleSerialConfig(input);
    }
  }

  // Jaga koneksi WebSocket
  if (WiFi.status() == WL_CONNECTED) {
    ws.loop();
    if (!wsConnected && millis() - lastReconnect > 3000) {
      lastReconnect = millis();
      connectWebSocket();
    }
    if (wsConnected && millis() - lastHeartbeat > 15000) {
      lastHeartbeat = millis();
      ws.sendTXT("{\"type\":\"hb\"}");
    }
  }

  // ---- Zero-OTA VM tick ----
  if (vm.loaded && vm.running) {
    if (!vm.setupDone) {
      vmSendLog("[VM] Menjalankan setup()...");
      vmRunBlock(vm.setupJson.c_str());
      vm.setupDone = true;
      vmSendLog("[VM] setup() selesai. Memulai loop()...");
    }
    unsigned long t0 = millis();
    vmRunBlock(vm.loopJson.c_str());
    if (millis() - t0 > 8000) {
      vmSendLog("[VM] WATCHDOG: loop() timeout, dihentikan.");
      vm.running = false;
    }
  }
}

// ============ Handle Config dari Serial ============

void handleSerialConfig(String json) {
  Serial.println("[FA] Menerima konfigurasi...");

  String ssid = extractJSON(json, "ssid");
  String pass = extractJSON(json, "pass");
  String key = extractJSON(json, "key");
  String server = extractJSON(json, "server");
  String deviceId = extractJSON(json, "deviceId");
  String pubkey = extractJSON(json, "pubkey");
  pubkey.replace("\\n", "\n"); // Unescape newlines for PEM format

  if (ssid.length() == 0 || key.length() == 0) {
    Serial.println("[FA] ERROR: SSID dan Key wajib diisi!");
    return;
  }

  prefs.begin("fa-iot", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("key", key);
  prefs.putString("server", server.length() > 0 ? server : "fa-lectric.com");
  prefs.putString(
      "deviceId",
      deviceId); // opsional, kosong = mode 1 perangkat (data di root)
  if (pubkey.length() > 0) {
    prefs.putString("pubkey", pubkey);
  }
  prefs.end();

  storedSSID = ssid;
  storedPass = pass;
  storedKey = key;
  storedServer = server.length() > 0 ? server : "fa-lectric.com";
  storedDeviceId = deviceId;

  Serial.println("[FA] Konfigurasi tersimpan!");
  if (pubkey.length() > 0) {
    Serial.println("[FA] Kunci publik ECDSA P-256 disimpan.");
  }
  Serial.println("[FA] SSID: " + storedSSID);
  Serial.println("[FA] Server: " + storedServer);
  if (storedDeviceId.length() > 0)
    Serial.println("[FA] Device ID: " + storedDeviceId);

  connectWiFi();
}

// ============ Connect WiFi ============

void connectWiFi() {
  Serial.print("[FA] Menghubungkan ke WiFi: " + storedSSID);
  WiFi.begin(storedSSID.c_str(), storedPass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[FA] WiFi Connected!");
    Serial.println("[FA] IP: " + WiFi.localIP().toString());
    connectWebSocket();
  } else {
    Serial.println();
    Serial.println("[FA] ERROR: Gagal connect WiFi! Periksa SSID/Password.");
    Serial.println("[FA] Menunggu konfigurasi ulang via Serial...");
  }
}

// ============ WebSocket ============

void connectWebSocket() {
  String path = "/ws?key=" + storedKey;
  if (storedWsPort == 443) {
    ws.beginSSL(storedServer.c_str(), 443, path.c_str());
  } else {
    ws.begin(storedServer.c_str(), storedWsPort, path.c_str());
  }
  ws.onEvent(wsEvent);
  ws.setReconnectInterval(3000);
  Serial.println("[FA] Menghubungkan ke server (WebSocket)...");
}

void wsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_CONNECTED:
    wsConnected = true;
    Serial.println("[FA] Terhubung ke server. Siap menerima perintah OTA & Program.");
    break;
  case WStype_DISCONNECTED:
    wsConnected = false;
    Serial.println("[FA] Koneksi server terputus.");
    break;
  case WStype_TEXT: {
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, payload, length)) return;
    const char *mtype = doc["type"];
    if (!mtype) break;

    // OTA Update
    if (strcmp(mtype, "ota") == 0) {
      const char *url = doc["url"];
      const char *ver = doc["version"] | "";
      if (url) performOTA(url, ver);
      break;
    }

    // Sync awal — cek apakah ada __PROGRAM__ yang sudah tersimpan
    if (strcmp(mtype, "sync") == 0) {
      JsonObject data = doc["data"];
      if (data.containsKey("__PROGRAM__")) {
        JsonObject prog = data["__PROGRAM__"];
        const char* status = prog["status"] | "";
        if (strcmp(status, "running") == 0) {
          String sj, lj, vj;
          serializeJson(prog["setup"], sj);
          serializeJson(prog["loop"],  lj);
          serializeJson(prog["vars"],  vj);
          vmLoadProgram(sj.c_str(), lj.c_str(), vj.c_str());
          Serial.println("[VM] Program dipulihkan dari sync.");
        }
      }
      break;
    }

    // Update realtime — deteksi __PROGRAM__ berubah
    if (strcmp(mtype, "update") == 0) {
      const char *key = doc["key"];
      if (key && strcmp(key, "__PROGRAM__") == 0) {
        DynamicJsonDocument progDoc(2048);
        String val = doc["value"].as<String>();
        if (!deserializeJson(progDoc, val)) {
          const char* status = progDoc["status"] | "";
          if (strcmp(status, "running") == 0) {
            String sj, lj, vj;
            serializeJson(progDoc["setup"], sj);
            serializeJson(progDoc["loop"],  lj);
            serializeJson(progDoc["vars"],  vj);
            vmLoadProgram(sj.c_str(), lj.c_str(), vj.c_str());
            Serial.println("[VM] Program baru diterima dari dashboard!");
          } else if (strcmp(status, "stopped") == 0) {
            vm.running = false;
            Serial.println("[VM] Program dihentikan dari dashboard.");
          }
        }
      }
      break;
    }
    break;
  }
  default:
    break;
  }
}

// ============ Report progres OTA ke aplikasi (via WebSocket) ============

void reportOTA(const char *phase, int percent, const char *message) {
  if (!wsConnected)
    return;
  String msg = "{\"type\":\"ota_status\",\"phase\":\"" + String(phase) + "\"";
  if (percent >= 0)
    msg += ",\"percent\":" + String(percent);
  if (message && strlen(message) > 0)
    msg += ",\"message\":\"" + String(message) + "\"";
  msg += "}";
  ws.sendTXT(msg);
  ws.loop();
}

// ============ OTA Update ============

void performOTA(const char *url, const char *version) {
  Serial.println("[FA] OTA dimulai. Versi: " + String(version));
  reportOTA("downloading", 0, "");

  HTTPClient http;
  http.begin(url);
  http.setTimeout(30000);
  int code = http.GET();

  if (code != 200) {
    Serial.println("[FA] OTA gagal. HTTP code: " + String(code));
    reportOTA("failed", -1, "Gagal mengunduh firmware");
    http.end();
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    reportOTA("failed", -1, "Ukuran firmware tidak valid");
    http.end();
    return;
  }

  if (!Update.begin(contentLength)) {
    reportOTA("failed", -1, "Memori tidak cukup untuk update");
    http.end();
    return;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buff[1024];
  size_t written = 0;
  int lastPercent = -1;
  unsigned long lastReport = millis();

  while (http.connected() && written < (size_t)contentLength) {
    size_t avail = stream->available();
    if (avail) {
      int readBytes = stream->readBytes(
          buff, ((avail > sizeof(buff)) ? sizeof(buff) : avail));
      Update.write(buff, readBytes);
      written += readBytes;

      int percent = (int)((written * 100) / contentLength);
      if (percent != lastPercent &&
          (percent - lastPercent >= 5 || millis() - lastReport > 800)) {
        lastPercent = percent;
        lastReport = millis();
        Serial.println("[FA] OTA " + String(percent) + "%");
        reportOTA("downloading", percent, "");
      }
    }
    delay(1);
  }

  if (written != (size_t)contentLength) {
    Update.abort();
    reportOTA("failed", -1, "Unduhan tidak lengkap");
    http.end();
    return;
  }

  reportOTA("writing", 100, "");

  if (!Update.end(true)) {
    reportOTA("failed", -1, "Gagal menulis firmware");
    http.end();
    return;
  }

  http.end();
  Serial.println("[FA] OTA berhasil. Restart...");
  reportOTA("success", 100, "Update berhasil");
  delay(1500);
  ESP.restart();
}

// ============ Helper: Extract value dari JSON string ============

String extractJSON(String json, String key) {
  String search = "\"" + key + "\":\"";
  int start = json.indexOf(search);
  if (start < 0) return "";
  start += search.length();
  int end = json.indexOf("\"", start);
  if (end < 0) return "";
  return json.substring(start, end);
}

// ============================================================
// ZERO-OTA VM INTERPRETER
// ============================================================

void vmSetVar(const char* name, float val) {
  for (uint8_t i = 0; i < vm.varCount; i++) {
    if (strcmp(vm.vars[i].name, name) == 0) { vm.vars[i].value = val; return; }
  }
  if (vm.varCount < 20) {
    strncpy(vm.vars[vm.varCount].name, name, 23);
    vm.vars[vm.varCount].value = val;
    vm.varCount++;
  }
}

float vmGetVar(const char* name) {
  for (uint8_t i = 0; i < vm.varCount; i++) {
    if (strcmp(vm.vars[i].name, name) == 0) return vm.vars[i].value;
  }
  return 0.0f;
}

bool vmEvalCondition(const char* leftName, const char* op, float rightVal) {
  float l = vmGetVar(leftName);
  if (strcmp(op, ">")  == 0) return l >  rightVal;
  if (strcmp(op, "<")  == 0) return l <  rightVal;
  if (strcmp(op, ">=") == 0) return l >= rightVal;
  if (strcmp(op, "<=") == 0) return l <= rightVal;
  if (strcmp(op, "==") == 0) return l == rightVal;
  if (strcmp(op, "!=") == 0) return l != rightVal;
  return false;
}

void vmSendLog(const char* msg) {
  Serial.println(msg);
  if (!wsConnected) return;
  String fk = storedDeviceId.length() > 0
    ? storedDeviceId + "/__PROGRAM_LOG__"
    : "__PROGRAM_LOG__";
  String escaped = String(msg);
  escaped.replace("\"", "'");
  String wsMsg = "{\"type\":\"set\",\"key\":\"" + fk + "\",\"value\":\"" + escaped + "\"}";
  ws.sendTXT(wsMsg);
}

void vmExecInstr(JsonArray& instr) {
  if (instr.size() == 0) return;
  String op = instr[0].as<String>();

  if (op == "WRITE") {
    pinMode(instr[1].as<int>(), OUTPUT);
    digitalWrite(instr[1].as<int>(), instr[2].as<int>() ? HIGH : LOW);
  }
  else if (op == "WRITE_A") {
    analogWrite(instr[1].as<int>(), constrain(instr[2].as<int>(), 0, 255));
  }
  else if (op == "PIN_MODE") {
    pinMode(instr[1].as<int>(), String(instr[2].as<const char*>()) == "OUTPUT" ? OUTPUT : INPUT);
  }
  else if (op == "READ_A") {
    float val = (float)analogRead(instr[1].as<int>());
    vmSetVar(instr[2].as<const char*>(), val);
    if (wsConnected) {
      String fk = storedDeviceId.length() > 0
        ? storedDeviceId + "/" + String(instr[2].as<const char*>())
        : String(instr[2].as<const char*>());
      ws.sendTXT("{\"type\":\"set\",\"key\":\"" + fk + "\",\"value\":" + String(val, 0) + "}");
    }
  }
  else if (op == "READ_D") {
    vmSetVar(instr[2].as<const char*>(), (float)digitalRead(instr[1].as<int>()));
  }
  else if (op == "DELAY") {
    unsigned long ms = min((unsigned long)instr[1].as<unsigned long>(), (unsigned long)5000);
    unsigned long t0 = millis();
    while (millis() - t0 < ms) { ws.loop(); delay(10); }
  }
  else if (op == "SERIAL") {
    String msg = instr[1].as<String>();
    for (uint8_t i = 0; i < vm.varCount; i++) {
      if (msg == String(vm.vars[i].name)) { msg = String(vm.vars[i].value, 2); break; }
    }
    vmSendLog(("[SKETCH] " + msg).c_str());
  }
  else if (op == "SET_VAR") {
    vmSetVar(instr[1].as<const char*>(), instr[2].as<float>());
  }
  else if (op == "IF") {
    bool cond = vmEvalCondition(
      instr[1].as<const char*>(),
      instr[2].as<const char*>(),
      instr[3].as<float>()
    );
    JsonArray blk = cond ? instr[4].as<JsonArray>()
                         : (instr.size() > 5 ? instr[5].as<JsonArray>() : JsonArray());
    if (!blk.isNull()) {
      for (JsonVariant item : blk) {
        if (!item.is<JsonArray>()) continue;
        JsonArray sub = item.as<JsonArray>();
        vmExecInstr(sub);
        if (!vm.running) break;
      }
    }
  }
}

void vmRunBlock(const char* jsonArray) {
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, jsonArray) != DeserializationError::Ok) return;
  JsonArray arr = doc.as<JsonArray>();
  for (JsonVariant item : arr) {
    if (!item.is<JsonArray>()) continue;
    JsonArray instr = item.as<JsonArray>();
    vmExecInstr(instr);
    if (!vm.running) break;
  }
}

void vmLoadProgram(const char* setupJson, const char* loopJson, const char* varsJson) {
  vm.setupJson  = String(setupJson);
  vm.loopJson   = String(loopJson);
  vm.varCount   = 0;
  vm.setupDone  = false;
  vm.loaded     = true;
  vm.running    = true;

  StaticJsonDocument<512> varsDoc;
  if (!deserializeJson(varsDoc, varsJson)) {
    for (JsonPair kv : varsDoc.as<JsonObject>()) {
      vmSetVar(kv.key().c_str(), kv.value().as<float>());
    }
  }
  Serial.println("[VM] Program di-load. Var: " + String(vm.varCount));
}
