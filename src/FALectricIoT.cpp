/**
 * FA-Lectric-IoT Library — Implementation
 * (c) 2026 Fachrain Azis - FA-LECTRIC
 */

#include "FALectricIoT.h"

// Global instance
FALectricIoT fa;

// Static reference for WebSocket callback
static FALectricIoT* _instance = nullptr;

FALectricIoT::FALectricIoT() {
  _connected = false;
  _lastReconnect = 0;
  _subCount = 0;
  _host = "fa-lectric.com";
  _port = 80;  // Lewat NGINX (/ws) — tidak perlu buka port internal ke internet
  _deviceKey = "";
  _otaEnabled = true;
  _instance = this;
}

void FALectricIoT::begin() {
  // Baca kredensial dari NVS (disimpan oleh OTA_Setup saat flash awal).
  // Cocok dipakai program yang dikirim via OTA: WiFi & key tidak perlu di-hardcode.
#ifdef ESP32
  Preferences prefs;
  prefs.begin("fa-iot", true); // read-only
  _nvsSsid = prefs.getString("ssid", "");
  _nvsPass = prefs.getString("pass", "");
  _nvsKey  = prefs.getString("key", "");
  _nvsHost = prefs.getString("server", "fa-lectric.com");
  uint16_t p = prefs.getUShort("wsport", _port);
  prefs.end();

  if (_nvsSsid.length() == 0 || _nvsKey.length() == 0) {
    Serial.println("[FA] ERROR: Kredensial NVS kosong. Lakukan Flash & Setup dulu.");
    return;
  }

  _deviceKey = _nvsKey.c_str();
  _host = _nvsHost.c_str();
  _port = p;
  _connectWiFi(_nvsSsid.c_str(), _nvsPass.c_str());
  _connectWebSocket();
#else
  // ESP8266 belum mendukung baca NVS otomatis — gunakan begin(ssid, pass, key).
  Serial.println("[FA] begin() tanpa argumen hanya didukung ESP32. Gunakan begin(ssid, pass, key).");
#endif
}

void FALectricIoT::begin(const char* ssid, const char* password, const char* deviceKey) {
  _deviceKey = deviceKey;
  _connectWiFi(ssid, password);
  _connectWebSocket();
}

void FALectricIoT::begin(const char* ssid, const char* password, const char* deviceKey, const char* host, uint16_t port) {
  _deviceKey = deviceKey;
  _host = host;
  _port = port;
  _connectWiFi(ssid, password);
  _connectWebSocket();
}

void FALectricIoT::setServer(const char* host, uint16_t port) {
  _host = host;
  _port = port;
}

void FALectricIoT::loop() {
  _ws.loop();

  // Auto-reconnect
  if (!_connected && millis() - _lastReconnect > FA_RECONNECT_INTERVAL) {
    _lastReconnect = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    } else {
      _connectWebSocket();
    }
  }
}

bool FALectricIoT::connected() {
  return _connected;
}

// ============ WiFi ============

void FALectricIoT::_connectWiFi(const char* ssid, const char* password) {
  Serial.print("[FA] Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!");
    Serial.print("[FA] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED!");
  }
}

// ============ WebSocket ============

void FALectricIoT::_connectWebSocket() {
  String path = "/ws?key=" + String(_deviceKey);
  _ws.begin(_host, _port, path.c_str());
  _ws.onEvent(_wsEvent);
  _ws.setReconnectInterval(FA_RECONNECT_INTERVAL);
}

void FALectricIoT::_wsEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (!_instance) return;

  switch (type) {
    case WStype_CONNECTED:
      _instance->_connected = true;
      Serial.println("[FA] Connected to server!");
      // Request initial data sync
      _instance->_ws.sendTXT("{\"type\":\"sync\"}");
      break;

    case WStype_DISCONNECTED:
      _instance->_connected = false;
      Serial.println("[FA] Disconnected.");
      break;

    case WStype_TEXT:
      _instance->_handleMessage(payload, length);
      break;

    default:
      break;
  }
}

void FALectricIoT::_handleMessage(uint8_t* payload, size_t length) {
  StaticJsonDocument<FA_JSON_BUFFER_SIZE> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  const char* type = doc["type"];
  if (!type) return;

  if (strcmp(type, "sync") == 0) {
    // Full data sync from server
    JsonObject data = doc["data"];
    for (JsonPair kv : data) {
      _cache[kv.key()] = kv.value();
    }
    Serial.println("[FA] Data synced.");
  }
  else if (strcmp(type, "update") == 0) {
    // Single key update from server (UI or other device changed it)
    const char* key = doc["key"];
    if (key) {
      _cache[key] = doc["value"];
      // Notify subscribers
      String val = doc["value"].as<String>();
      _notifySubscribers(key, val.c_str());
    }
  }
  else if (strcmp(type, "ota") == 0) {
    // Perintah update OTA dari aplikasi (dipicu user). Unduh & flash firmware baru.
    if (!_otaEnabled) return;
    const char* otaUrl = doc["url"];
    const char* otaVer = doc["version"] | "";
    if (otaUrl) {
      _performOTA(otaUrl, otaVer);
    }
  }
}

// ============ OTA (Over-The-Air Update) ============

void FALectricIoT::enableOTA(bool enable) {
  _otaEnabled = enable;
}

// Laporkan progres OTA ke server lewat WebSocket (ditampilkan realtime di aplikasi).
void FALectricIoT::_reportOTA(const char* phase, int percent, const char* message) {
  if (!_connected) return;
  String msg = "{\"type\":\"ota_status\",\"phase\":\"" + String(phase) + "\"";
  if (percent >= 0) msg += ",\"percent\":" + String(percent);
  if (message && strlen(message) > 0) msg += ",\"message\":\"" + String(message) + "\"";
  msg += "}";
  _ws.sendTXT(msg);
  _ws.loop(); // pastikan terkirim sebelum proses flash menyita CPU
}

void FALectricIoT::_performOTA(const char* url, const char* version) {
  Serial.println("[FA] OTA dimulai. Versi: " + String(version));
  _reportOTA("downloading", 0, "");

  HTTPClient http;
  http.begin(url);
  http.setTimeout(30000);
  int code = http.GET();

  if (code != 200) {
    Serial.println("[FA] OTA gagal. HTTP code: " + String(code));
    _reportOTA("failed", -1, "Gagal mengunduh firmware");
    http.end();
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    _reportOTA("failed", -1, "Ukuran firmware tidak valid");
    http.end();
    return;
  }

  if (!Update.begin(contentLength)) {
    _reportOTA("failed", -1, "Memori tidak cukup untuk update");
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buff[1024];
  size_t written = 0;
  int lastPercent = -1;
  unsigned long lastReport = millis();

  while (http.connected() && written < (size_t)contentLength) {
    size_t avail = stream->available();
    if (avail) {
      int readBytes = stream->readBytes(buff, ((avail > sizeof(buff)) ? sizeof(buff) : avail));
      Update.write(buff, readBytes);
      written += readBytes;

      int percent = (int)((written * 100) / contentLength);
      // Lapor tiap perubahan 5% atau tiap 800ms agar tidak membanjiri WebSocket
      if (percent != lastPercent && (percent - lastPercent >= 5 || millis() - lastReport > 800)) {
        lastPercent = percent;
        lastReport = millis();
        Serial.println("[FA] OTA " + String(percent) + "%");
        _reportOTA("downloading", percent, "");
      }
    }
    delay(1);
  }

  if (written != (size_t)contentLength) {
    Update.abort();
    _reportOTA("failed", -1, "Unduhan tidak lengkap");
    http.end();
    return;
  }

  _reportOTA("writing", 100, "");

  if (!Update.end(true)) {
    _reportOTA("failed", -1, "Gagal menulis firmware");
    http.end();
    return;
  }

  http.end();
  Serial.println("[FA] OTA berhasil. Restart...");
  _reportOTA("success", 100, "Update berhasil");
  delay(1500);
  ESP.restart();
}

// ============ Realtime Database — Write ============

void FALectricIoT::_sendSet(const char* key, const char* value) {
  if (!_connected) return;
  StaticJsonDocument<256> doc;
  doc["type"] = "set";
  doc["key"] = key;
  doc["value"] = serialized(value); // raw
  // Actually send as proper JSON
  String msg;
  serializeJson(doc, msg);
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char* key, float value) {
  _cache[key] = value;
  if (!_connected) return;
  String msg = "{\"type\":\"set\",\"key\":\"" + String(key) + "\",\"value\":" + String(value, 2) + "}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char* key, int value) {
  _cache[key] = value;
  if (!_connected) return;
  String msg = "{\"type\":\"set\",\"key\":\"" + String(key) + "\",\"value\":" + String(value) + "}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char* key, bool value) {
  _cache[key] = value;
  if (!_connected) return;
  String msg = "{\"type\":\"set\",\"key\":\"" + String(key) + "\",\"value\":" + (value ? "true" : "false") + "}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char* key, const char* value) {
  _cache[key] = value;
  if (!_connected) return;
  String msg = "{\"type\":\"set\",\"key\":\"" + String(key) + "\",\"value\":\"" + String(value) + "\"}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char* key, String value) {
  set(key, value.c_str());
}

// ============ Realtime Database — Read (from cache) ============

float FALectricIoT::getFloat(const char* key) {
  return _cache[key] | 0.0f;
}

int FALectricIoT::getInt(const char* key) {
  return _cache[key] | 0;
}

bool FALectricIoT::getBool(const char* key) {
  return _cache[key] | false;
}

String FALectricIoT::getString(const char* key) {
  const char* val = _cache[key];
  return val ? String(val) : "";
}

// ============ Collections — Push ============

void FALectricIoT::push(const char* collection, const char* key1, float val1) {
  if (!_connected) return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) + "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) + "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::push(const char* collection, const char* key1, float val1, const char* key2, float val2) {
  if (!_connected) return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) + "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) + ",\"" + String(key2) + "\":" + String(val2, 2) + "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::push(const char* collection, const char* key1, float val1, const char* key2, float val2, const char* key3, float val3) {
  if (!_connected) return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) + "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) + ",\"" + String(key2) + "\":" + String(val2, 2) + ",\"" + String(key3) + "\":" + String(val3, 2) + "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::push(const char* collection, const char* key1, float val1, const char* key2, float val2, const char* key3, float val3, const char* key4, float val4) {
  if (!_connected) return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) + "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) + ",\"" + String(key2) + "\":" + String(val2, 2) + ",\"" + String(key3) + "\":" + String(val3, 2) + ",\"" + String(key4) + "\":" + String(val4, 2) + "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::pushJSON(const char* collection, const char* jsonPayload) {
  if (!_connected) return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) + "\",\"data\":" + String(jsonPayload) + "}";
  _ws.sendTXT(msg);
}

// ============ Subscribe ============

void FALectricIoT::on(const char* key, FACallback callback) {
  if (_subCount >= FA_MAX_SUBSCRIPTIONS) return;
  _subs[_subCount].key = String(key);
  _subs[_subCount].callback = callback;
  _subCount++;
}

void FALectricIoT::_notifySubscribers(const char* key, const char* value) {
  for (uint8_t i = 0; i < _subCount; i++) {
    if (_subs[i].key == key) {
      _subs[i].callback(String(value));
    }
  }
}
