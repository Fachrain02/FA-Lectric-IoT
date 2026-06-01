/**
 * FA-LECTRIC IoT — OTA Setup Firmware (Bootstrap)
 *
 * Program awal yang di-flash via kabel (website fa-lectric.com/iot/flash).
 * Tugasnya:
 *   1. Terima konfigurasi WiFi + API Key via Serial (dari website saat flash).
 *   2. Simpan ke NVS (memori permanen).
 *   3. Connect WiFi + WebSocket ke server.
 *   4. Dengarkan perintah OTA dari aplikasi -> unduh & flash program baru
 *      (mis. BasicRealtimeDB, RelayControl, dll) TANPA kabel lagi.
 *
 * Board: ESP32 Dev Module (atau varian lain)
 * Partition Scheme: Default 4MB with spiffs (atau Minimal SPIFFS jika program
 * besar)
 *
 * Library yang dibutuhkan (install via Arduino Library Manager):
 *   - WebSockets by Markus Sattler  (arduinoWebSockets)
 *   - ArduinoJson by Benoit Blanchon
 *   (WiFi, HTTPClient, Update, Preferences sudah built-in ESP32)
 */

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

Preferences prefs;
WebSocketsClient ws;

String storedSSID = "";
String storedPass = "";
String storedKey = "";
String storedServer = "";
String storedDeviceId = "";
uint16_t storedWsPort = 443; // WSS via NGINX (HTTPS/SSL)

bool wsConnected = false;
unsigned long lastReconnect = 0;
unsigned long lastHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("[FA] ============================");
  Serial.println("[FA] FA-LECTRIC IoT OTA Setup v2.0");
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
    // Heartbeat non-blocking tiap 15 detik (tanda masih hidup)
    if (wsConnected && millis() - lastHeartbeat > 15000) {
      lastHeartbeat = millis();
      ws.sendTXT("{\"type\":\"hb\"}");
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
    Serial.println("[FA] Terhubung ke server. Siap menerima perintah OTA.");
    Serial.println("[FA] Menunggu program dari dashboard...");
    break;
  case WStype_DISCONNECTED:
    wsConnected = false;
    Serial.println("[FA] Koneksi server terputus.");
    break;
  case WStype_TEXT: {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, payload, length))
      return;
    const char *mtype = doc["type"];
    if (mtype && strcmp(mtype, "ota") == 0) {
      const char *url = doc["url"];
      const char *ver = doc["version"] | "";
      if (url)
        performOTA(url, ver);
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
  if (start < 0)
    return "";
  start += search.length();
  int end = json.indexOf("\"", start);
  if (end < 0)
    return "";
  return json.substring(start, end);
}
