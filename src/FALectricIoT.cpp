
#include "FALectricIoT.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include <base64.h>

FALectricIoT fa;

static FALectricIoT *_instance = nullptr;

FALectricIoT::FALectricIoT() {
  _connected = false;
  _lastReconnect = 0;
  _lastHeartbeat = 0;
  _subCount = 0;
  _host = "fa-lectric.com";
  _port = 443;
  _deviceKey = "";
  _otaEnabled = true;
  _currentFolder = "";
  _deviceId = "";
  _instance = this;
  // Init VM
  _vm.loaded    = false;
  _vm.running   = false;
  _vm.setupDone = false;
  _vm.varCount  = 0;
}

void FALectricIoT::begin() {
#ifdef ESP32
  Preferences prefs;
  prefs.begin("fa-iot", true);
  _nvsSsid = prefs.getString("ssid", "");
  _nvsPass = prefs.getString("pass", "");
  _nvsKey = prefs.getString("key", "");
  _nvsHost = prefs.getString("server", "fa-lectric.com");
  _nvsPubKey = prefs.getString("pubkey", "");
  uint16_t p = prefs.getUShort("wsport", _port);
  if (_deviceId.length() == 0) {
    String nvsDeviceId = prefs.getString("deviceId", "");
    nvsDeviceId.trim();
    while (nvsDeviceId.startsWith("/"))
      nvsDeviceId = nvsDeviceId.substring(1);
    while (nvsDeviceId.endsWith("/"))
      nvsDeviceId = nvsDeviceId.substring(0, nvsDeviceId.length() - 1);
    _deviceId = nvsDeviceId;
  }
  prefs.end();

  if (_nvsSsid.length() == 0 || _nvsKey.length() == 0) {
    Serial.println(
        "[FA] ERROR: Kredensial NVS kosong. Lakukan Flash & Setup dulu.");
    return;
  }

  _deviceKey = _nvsKey.c_str();
  _host = _nvsHost.c_str();
  _port = p;
  if (_deviceId.length() > 0)
    Serial.println("[FA] Device ID: " + _deviceId);
  _connectWiFi(_nvsSsid.c_str(), _nvsPass.c_str());
  _connectWebSocket();
#else
  Serial.println("[FA] begin() tanpa argumen hanya didukung ESP32. Gunakan "
                 "begin(ssid, pass, key).");
#endif
}

void FALectricIoT::begin(const char *ssid, const char *password,
                         const char *deviceKey) {
  _deviceKey = deviceKey;
  _connectWiFi(ssid, password);
  _connectWebSocket();
}

void FALectricIoT::begin(const char *ssid, const char *password,
                         const char *deviceKey, const char *deviceId) {
  _deviceId = deviceId;
  _deviceKey = deviceKey;
  _connectWiFi(ssid, password);
  _connectWebSocket();
}

void FALectricIoT::begin(const char *ssid, const char *password,
                         const char *deviceKey, const char *host,
                         uint16_t port) {
  _deviceKey = deviceKey;
  _host = host;
  _port = port;
  _connectWiFi(ssid, password);
  _connectWebSocket();
}

void FALectricIoT::setServer(const char *host, uint16_t port) {
  _host = host;
  _port = port;
}

void FALectricIoT::device(const char *deviceId) {
  String d = String(deviceId);
  d.trim();
  while (d.startsWith("/"))
    d = d.substring(1);
  while (d.endsWith("/"))
    d = d.substring(0, d.length() - 1);
  _deviceId = d;
}

void FALectricIoT::folder(const char *path) {
  String p = String(path);
  p.trim();
  while (p.startsWith("/"))
    p = p.substring(1);
  while (p.endsWith("/"))
    p = p.substring(0, p.length() - 1);
  _currentFolder = p;
}

String FALectricIoT::_fullKey(const char *key) {
  String full = "";
  if (_deviceId.length() > 0)
    full = _deviceId;
  if (_currentFolder.length() > 0)
    full = (full.length() > 0 ? full + "/" : "") + _currentFolder;
  full = (full.length() > 0 ? full + "/" : "") + String(key);
  return full;
}

void FALectricIoT::loop() {
  _ws.loop();

  if (_connected && millis() - _lastHeartbeat > FA_HEARTBEAT_INTERVAL) {
    _lastHeartbeat = millis();
    String hbMsg = "{\"type\":\"hb\"";
    if (_deviceId.length() > 0) {
      hbMsg += ",\"dev\":\"" + _deviceId + "\"";
    }
    hbMsg += "}";
    _ws.sendTXT(hbMsg);
  }

  if (!_connected && millis() - _lastReconnect > FA_RECONNECT_INTERVAL) {
    _lastReconnect = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    } else {
      _connectWebSocket();
    }
  }

  // ---- Zero-OTA VM tick ----
  if (_vm.loaded && _vm.running) {
    if (!_vm.setupDone) {
      _vmSendLog("[VM] Menjalankan setup()...");
      _vmRunBlock(_vm.setupJson.c_str());
      _vm.setupDone = true;
      _vmSendLog("[VM] setup() selesai. Memulai loop()...");
    }
    unsigned long t0 = millis();
    _vmRunBlock(_vm.loopJson.c_str());
    if (millis() - t0 > FA_VM_WATCHDOG_MS) {
      _vmSendLog("[VM] WATCHDOG: loop() terlalu lama, dihentikan sementara.");
    }
  }
}

bool FALectricIoT::connected() { return _connected; }

void FALectricIoT::_connectWiFi(const char *ssid, const char *password) {
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

void FALectricIoT::_connectWebSocket() {
  String path = "/ws?key=" + String(_deviceKey);
  if (_deviceId.length() > 0) {
    path += "&dev=" + _deviceId;
  }
  if (_port == 443) {
    _ws.beginSSL(_host, 443, path.c_str());
  } else {
    _ws.begin(_host, _port, path.c_str());
  }
  _ws.onEvent(_wsEvent);
  _ws.setReconnectInterval(FA_RECONNECT_INTERVAL);
}

void FALectricIoT::_wsEvent(WStype_t type, uint8_t *payload, size_t length) {
  if (!_instance)
    return;

  switch (type) {
  case WStype_CONNECTED:
    _instance->_connected = true;
    Serial.println("[FA] Connected to server!");

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

void FALectricIoT::_handleMessage(uint8_t *payload, size_t length) {
  StaticJsonDocument<FA_JSON_BUFFER_SIZE> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err)
    return;

  const char *type = doc["type"];
  if (!type)
    return;

  if (strcmp(type, "sync") == 0) {
    JsonObject data = doc["data"];
    for (JsonPair kv : data) {
      _cache[kv.key()] = kv.value();
    }
    Serial.println("[FA] Data synced.");

    // Cek apakah ada program yang sudah tersimpan di data
    if (data.containsKey("__PROGRAM__")) {
      JsonObject prog = data["__PROGRAM__"];
      const char* status = prog["status"] | "";
      if (strcmp(status, "running") == 0) {
        String setupStr, loopStr, varsStr;
        serializeJson(prog["setup"], setupStr);
        serializeJson(prog["loop"],  loopStr);
        serializeJson(prog["vars"],  varsStr);
        _vmLoadProgram(setupStr.c_str(), loopStr.c_str(), varsStr.c_str());
      }
    }

  } else if (strcmp(type, "update") == 0) {
    const char *key = doc["key"];
    if (key) {
      _cache[key] = doc["value"];
      String val = doc["value"].as<String>();
      _notifySubscribers(key, val.c_str());

      // Deteksi perubahan __PROGRAM__ secara real-time
      if (strcmp(key, "__PROGRAM__") == 0) {
        StaticJsonDocument<FA_JSON_BUFFER_SIZE> progDoc;
        if (!deserializeJson(progDoc, val)) {
          const char* status = progDoc["status"] | "";
          if (strcmp(status, "running") == 0) {
            String setupStr, loopStr, varsStr;
            serializeJson(progDoc["setup"], setupStr);
            serializeJson(progDoc["loop"],  loopStr);
            serializeJson(progDoc["vars"],  varsStr);
            _vmLoadProgram(setupStr.c_str(), loopStr.c_str(), varsStr.c_str());
            Serial.println("[VM] Program baru diterima!");
          } else if (strcmp(status, "stopped") == 0) {
            _vm.running = false;
            Serial.println("[VM] Program dihentikan dari dashboard.");
            _vmSendLog("[VM] Program dihentikan.");
          }
        }
      }
    }
  } else if (strcmp(type, "ota") == 0) {
    if (!_otaEnabled)
      return;
    const char *otaUrl = doc["url"];
    const char *otaVer = doc["version"] | "";
    const char *sig    = doc["signature"] | "";
    _otaSignature = String(sig);
    if (otaUrl) {
      _performOTA(otaUrl, otaVer);
    }
  }
}

void FALectricIoT::enableOTA(bool enable) { _otaEnabled = enable; }

void FALectricIoT::_reportOTA(const char *phase, int percent,
                              const char *message) {
  if (!_connected)
    return;
  String msg = "{\"type\":\"ota_status\",\"phase\":\"" + String(phase) + "\"";
  if (_deviceId.length() > 0) {
    msg += ",\"dev\":\"" + _deviceId + "\"";
  }
  if (percent >= 0)
    msg += ",\"percent\":" + String(percent);
  if (message && strlen(message) > 0)
    msg += ",\"message\":\"" + String(message) + "\"";
  msg += "}";
  _ws.sendTXT(msg);
  _ws.loop();
}

void FALectricIoT::_performOTA(const char *url, const char *version) {
  Serial.println("[FA] OTA dimulai. Versi: " + String(version));
  _reportOTA("downloading", 0, "");

  HTTPClient http;
  bool isHttps = String(url).startsWith("https://");

#ifdef ESP32
#include <WiFiClientSecure.h>
  WiFiClientSecure secureClient;
  WiFiClient plainClient;

  if (isHttps) {
    secureClient.setInsecure();
    http.begin(secureClient, url);
  } else {
    http.begin(plainClient, url);
  }

  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
#elif defined(ESP8266)
#include <WiFiClientSecure.h>
  std::unique_ptr<BearSSL::WiFiClientSecure> secureClient(
      new BearSSL::WiFiClientSecure);
  WiFiClient plainClient;

  if (isHttps) {
    secureClient->setInsecure();
    http.begin(*secureClient, url);
  } else {
    http.begin(plainClient, url);
  }

  http.setFollowRedirects(true);
#endif

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

  mbedtls_sha256_context sha_ctx;
  mbedtls_sha256_init(&sha_ctx);
  mbedtls_sha256_starts_ret(&sha_ctx, 0);

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
      mbedtls_sha256_update_ret(&sha_ctx, buff,
                                readBytes);
      written += readBytes;

      int percent = (int)((written * 100) / contentLength);

      if (percent != lastPercent &&
          (percent - lastPercent >= 5 || millis() - lastReport > 800)) {
        lastPercent = percent;
        lastReport = millis();
        Serial.println("[FA] OTA " + String(percent) + "%");
        _reportOTA("downloading", percent, "");
      }
    }
    delay(1);
  }

  if (written != (size_t)contentLength) {
    mbedtls_sha256_free(&sha_ctx);
    Update.abort();
    _reportOTA("failed", -1, "Unduhan tidak lengkap");
    http.end();
    return;
  }

  uint8_t hash[32];
  mbedtls_sha256_finish_ret(&sha_ctx, hash);
  mbedtls_sha256_free(&sha_ctx);

  if (_nvsPubKey.length() > 0) {
    Serial.println("[FA] Kunci publik ditemukan di NVS. Memverifikasi tanda "
                   "tangan digital...");
    if (!_verifySignature(hash, 32, _otaSignature, _nvsPubKey)) {
      Update.abort();
      _reportOTA("failed", -1, "Tanda tangan digital palsu / tidak valid!");
      http.end();
      return;
    }
  } else {
    Serial.println("[FA] Peringatan: Tidak ada Kunci Publik di NVS. Melewati "
                   "verifikasi (mode tidak terkunci).");
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

void FALectricIoT::_sendSet(const char *key, const char *value) {
  if (!_connected)
    return;
  StaticJsonDocument<256> doc;
  doc["type"] = "set";
  doc["key"] = key;
  doc["value"] = serialized(value);

  String msg;
  serializeJson(doc, msg);
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char *key, float value) {
  String fk = _fullKey(key);
  _cache[fk] = value;
  if (!_connected)
    return;
  String msg = "{\"type\":\"set\",\"key\":\"" + fk +
               "\",\"value\":" + String(value, 2) + "}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char *key, double value) {
  set(key, (float)value);
}

void FALectricIoT::set(const char *key, int value) {
  String fk = _fullKey(key);
  _cache[fk] = value;
  if (!_connected)
    return;
  String msg = "{\"type\":\"set\",\"key\":\"" + fk +
               "\",\"value\":" + String(value) + "}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char *key, bool value) {
  String fk = _fullKey(key);
  _cache[fk] = value;
  if (!_connected)
    return;
  String msg = "{\"type\":\"set\",\"key\":\"" + fk +
               "\",\"value\":" + (value ? "true" : "false") + "}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char *key, const char *value) {
  String fk = _fullKey(key);
  _cache[fk] = value;
  if (!_connected)
    return;
  String msg = "{\"type\":\"set\",\"key\":\"" + fk + "\",\"value\":\"" +
               String(value) + "\"}";
  _ws.sendTXT(msg);
}

void FALectricIoT::set(const char *key, String value) {
  set(key, value.c_str());
}

float FALectricIoT::getFloat(const char *key) {
  return _cache[_fullKey(key)] | 0.0f;
}

int FALectricIoT::getInt(const char *key) { return _cache[_fullKey(key)] | 0; }

bool FALectricIoT::getBool(const char *key) {
  return _cache[_fullKey(key)] | false;
}

String FALectricIoT::getString(const char *key) {
  const char *val = _cache[_fullKey(key)];
  return val ? String(val) : "";
}

void FALectricIoT::push(const char *collection, const char *key1, float val1) {
  if (!_connected)
    return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) +
               "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) +
               "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::push(const char *collection, const char *key1, float val1,
                        const char *key2, float val2) {
  if (!_connected)
    return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) +
               "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) +
               ",\"" + String(key2) + "\":" + String(val2, 2) + "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::push(const char *collection, const char *key1, float val1,
                        const char *key2, float val2, const char *key3,
                        float val3) {
  if (!_connected)
    return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) +
               "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) +
               ",\"" + String(key2) + "\":" + String(val2, 2) + ",\"" +
               String(key3) + "\":" + String(val3, 2) + "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::push(const char *collection, const char *key1, float val1,
                        const char *key2, float val2, const char *key3,
                        float val3, const char *key4, float val4) {
  if (!_connected)
    return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) +
               "\",\"data\":{\"" + String(key1) + "\":" + String(val1, 2) +
               ",\"" + String(key2) + "\":" + String(val2, 2) + ",\"" +
               String(key3) + "\":" + String(val3, 2) + ",\"" + String(key4) +
               "\":" + String(val4, 2) + "}}";
  _ws.sendTXT(msg);
}

void FALectricIoT::pushJSON(const char *collection, const char *jsonPayload) {
  if (!_connected)
    return;
  String msg = "{\"type\":\"push\",\"collection\":\"" + String(collection) +
               "\",\"data\":" + String(jsonPayload) + "}";
  _ws.sendTXT(msg);
}

void FALectricIoT::on(const char *key, FACallback callback) {
  if (_subCount >= FA_MAX_SUBSCRIPTIONS)
    return;
  _subs[_subCount].key = String(key);
  _subs[_subCount].callback = callback;
  _subCount++;
}

void FALectricIoT::_notifySubscribers(const char *key, const char *value) {
  for (uint8_t i = 0; i < _subCount; i++) {
    if (_subs[i].key == key) {
      _subs[i].callback(String(value));
    }
  }
}

#ifdef ESP32
#include "mbedtls/aes.h"
#include <base64.h>

String FALectricIoT::encryptAES(String plaintext, const char *key32) {
  uint8_t iv[16];
  esp_fill_random(iv, 16);

  int plaintextLen = plaintext.length();
  int paddingValue = 16 - (plaintextLen % 16);
  int paddedLen = plaintextLen + paddingValue;
  uint8_t inputBuffer[paddedLen];
  uint8_t outputBuffer[paddedLen];

  memcpy(inputBuffer, plaintext.c_str(), plaintextLen);
  for (int i = plaintextLen; i < paddedLen; i++) {
    inputBuffer[i] = paddingValue;
  }

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, (const unsigned char *)key32, 256);

  uint8_t tempIv[16];
  memcpy(tempIv, iv, 16);

  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, tempIv,
                        inputBuffer, outputBuffer);
  mbedtls_aes_free(&aes);

  String base64IV = base64::encode(iv, 16);
  base64IV.replace("\n", "");
  base64IV.replace("\r", "");

  String base64Cipher = base64::encode(outputBuffer, paddedLen);
  base64Cipher.replace("\n", "");
  base64Cipher.replace("\r", "");

  return base64IV + ":" + base64Cipher;
}

String FALectricIoT::decryptAES(String ciphertext, const char *key32) {
  int colonIndex = ciphertext.indexOf(':');
  if (colonIndex == -1)
    return "";

  String base64IV = ciphertext.substring(0, colonIndex);
  String base64Cipher = ciphertext.substring(colonIndex + 1);

  String decodedIVStr = base64::decode(base64IV);
  String decodedCipherStr = base64::decode(base64Cipher);

  int cipherLen = decodedCipherStr.length();
  if (cipherLen % 16 != 0 || cipherLen == 0)
    return "";

  uint8_t iv[16];
  memcpy(iv, decodedIVStr.c_str(), 16);

  uint8_t inputBuffer[cipherLen];
  uint8_t outputBuffer[cipherLen];
  memcpy(inputBuffer, decodedCipherStr.c_str(), cipherLen);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, (const unsigned char *)key32, 256);

  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, iv, inputBuffer,
                        outputBuffer);
  mbedtls_aes_free(&aes);

  int paddingValue = outputBuffer[cipherLen - 1];
  if (paddingValue < 1 || paddingValue > 16)
    return "";
  int plaintextLen = cipherLen - paddingValue;

  String plaintext = "";
  for (int i = 0; i < plaintextLen; i++) {
    plaintext += (char)outputBuffer[i];
  }

  return plaintext;
}
#endif

bool FALectricIoT::_verifySignature(const uint8_t *hash, size_t hashLen,
                                    const String &signatureBase64,
                                    const String &pubKeyPEM) {
  if (pubKeyPEM.length() == 0) {
    Serial.println("[FA] [Crypto] Error: Public key NVS kosong!");
    return false;
  }
  if (signatureBase64.length() == 0) {
    Serial.println("[FA] [Crypto] Error: Signature kosong!");
    return false;
  }

  String sigBin = base64::decode(signatureBase64);
  size_t sigLen = sigBin.length();
  if (sigLen == 0) {
    Serial.println("[FA] [Crypto] Error: Gagal decode signature Base64!");
    return false;
  }

  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);

  int ret = mbedtls_pk_parse_public_key(
      &pk, (const unsigned char *)pubKeyPEM.c_str(), pubKeyPEM.length() + 1);
  if (ret != 0) {
    Serial.printf("[FA] [Crypto] Error: Gagal parse public key PEM! mbedtls "
                  "code: -0x%04X\n",
                  -ret);
    mbedtls_pk_free(&pk);
    return false;
  }

  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, hashLen,
                          (const unsigned char *)sigBin.c_str(), sigLen);
  mbedtls_pk_free(&pk);

  if (ret != 0) {
    Serial.printf("[FA] [Crypto] Error: Tanda tangan digital PALSU/INVALID! "
                  "mbedtls code: -0x%04X\n",
                  -ret);
    return false;
  }

  Serial.println("[FA] [Crypto] Tanda tangan digital SAH dan terverifikasi!");
  return true;
}

// ============================================================
// ZERO-OTA VM INTERPRETER IMPLEMENTATION
// ============================================================

void FALectricIoT::_vmLoadProgram(const char* setupJson, const char* loopJson, const char* varsJson) {
  _vm.setupJson  = String(setupJson);
  _vm.loopJson   = String(loopJson);
  _vm.varCount   = 0;
  _vm.setupDone  = false;
  _vm.loaded     = true;
  _vm.running    = true;

  // Muat variabel awal dari JSON object {"name": value, ...}
  StaticJsonDocument<512> varsDoc;
  if (!deserializeJson(varsDoc, varsJson)) {
    for (JsonPair kv : varsDoc.as<JsonObject>()) {
      _vmSetVar(kv.key().c_str(), kv.value().as<float>());
    }
  }
  Serial.println("[VM] Program di-load. Variabel: " + String(_vm.varCount));
}

// ---------- Jalankan satu blok instruksi (setup atau loop) ----------
void FALectricIoT::_vmRunBlock(const char* jsonArray) {
  // Parse array instruksi dari JSON string
  // Gunakan DynamicJsonDocument agar tidak overflow di stack
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, jsonArray) != DeserializationError::Ok) return;

  JsonArray arr = doc.as<JsonArray>();
  for (JsonVariant item : arr) {
    if (!item.is<JsonArray>()) continue;
    JsonArray instr = item.as<JsonArray>();
    _vmExecInstr(instr);

    // Cek apakah program di-stop dari dashboard selama eksekusi
    if (!_vm.running) break;
  }
}

// ---------- Eksekusi satu instruksi ----------
void FALectricIoT::_vmExecInstr(JsonArray& instr) {
  if (instr.size() == 0) return;
  String op = instr[0].as<String>();

  // WRITE — digitalWrite
  if (op == "WRITE") {
    int pin = instr[1].as<int>();
    int val = instr[2].as<int>();
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val ? HIGH : LOW);
    return;
  }

  // WRITE_A — analogWrite (PWM)
  if (op == "WRITE_A") {
    int pin = instr[1].as<int>();
    int val = instr[2].as<int>();
    analogWrite(pin, constrain(val, 0, 255));
    return;
  }

  // PIN_MODE
  if (op == "PIN_MODE") {
    int pin = instr[1].as<int>();
    String mode = instr[2].as<String>();
    pinMode(pin, mode == "OUTPUT" ? OUTPUT : INPUT);
    return;
  }

  // READ_A — analogRead → simpan ke variabel
  if (op == "READ_A") {
    int pin = instr[1].as<int>();
    const char* varName = instr[2].as<const char*>();
    float val = (float)analogRead(pin);
    _vmSetVar(varName, val);
    // Kirim nilai sensor ke realtime DB agar widget Display bisa membacanya
    if (_connected) {
      String fk = _fullKey(varName);
      String msg = "{\"type\":\"set\",\"key\":\"" + fk + "\",\"value\":" + String(val, 0) + "}";
      _ws.sendTXT(msg);
    }
    return;
  }

  // READ_D — digitalRead → simpan ke variabel
  if (op == "READ_D") {
    int pin = instr[1].as<int>();
    const char* varName = instr[2].as<const char*>();
    float val = (float)digitalRead(pin);
    _vmSetVar(varName, val);
    return;
  }

  // DELAY
  if (op == "DELAY") {
    unsigned long ms = instr[1].as<unsigned long>();
    // Batasi delay maks 5 detik untuk keamanan
    ms = min(ms, (unsigned long)5000);
    unsigned long t0 = millis();
    while (millis() - t0 < ms) {
      _ws.loop(); // Tetap proses WebSocket selama delay
      delay(10);
    }
    return;
  }

  // SERIAL — Serial.println
  if (op == "SERIAL") {
    String msg = instr[1].as<String>();
    // Ganti nama variabel dengan nilainya jika ada
    for (uint8_t i = 0; i < _vm.varCount; i++) {
      String varName = String(_vm.vars[i].name);
      if (msg == varName) {
        msg = String(_vm.vars[i].value, 2);
        break;
      }
    }
    Serial.println("[SKETCH] " + msg);
    _vmSendLog("[SKETCH] " + msg);
    return;
  }

  // SET_VAR — assignment variabel
  if (op == "SET_VAR") {
    const char* name = instr[1].as<const char*>();
    float val = instr[2].as<float>();
    _vmSetVar(name, val);
    return;
  }

  // IF — percabangan kondisional
  if (op == "IF") {
    const char* leftVar  = instr[1].as<const char*>();
    const char* opStr    = instr[2].as<const char*>();
    float rightVal       = instr[3].as<float>();
    bool cond = _vmEvalCondition(leftVar, opStr, rightVal);

    // Jalankan blok true atau else
    JsonArray targetBlock = cond ? instr[4].as<JsonArray>() : (instr.size() > 5 ? instr[5].as<JsonArray>() : JsonArray());
    if (!targetBlock.isNull()) {
      for (JsonVariant item : targetBlock) {
        if (!item.is<JsonArray>()) continue;
        JsonArray sub = item.as<JsonArray>();
        _vmExecInstr(sub);
        if (!_vm.running) break;
      }
    }
    return;
  }
}

// ---------- Helpers variabel ----------
void FALectricIoT::_vmSetVar(const char* name, float val) {
  for (uint8_t i = 0; i < _vm.varCount; i++) {
    if (strcmp(_vm.vars[i].name, name) == 0) {
      _vm.vars[i].value = val;
      return;
    }
  }
  if (_vm.varCount < FA_VM_MAX_VARS) {
    strncpy(_vm.vars[_vm.varCount].name, name, sizeof(_vm.vars[0].name) - 1);
    _vm.vars[_vm.varCount].value = val;
    _vm.varCount++;
  }
}

float FALectricIoT::_vmGetVar(const char* name) {
  for (uint8_t i = 0; i < _vm.varCount; i++) {
    if (strcmp(_vm.vars[i].name, name) == 0) {
      return _vm.vars[i].value;
    }
  }
  return 0.0f;
}

// ---------- Evaluasi kondisi IF ----------
bool FALectricIoT::_vmEvalCondition(const char* leftName, const char* op, float rightVal) {
  float leftVal = _vmGetVar(leftName);
  if (strcmp(op, ">")  == 0) return leftVal >  rightVal;
  if (strcmp(op, "<")  == 0) return leftVal <  rightVal;
  if (strcmp(op, ">=") == 0) return leftVal >= rightVal;
  if (strcmp(op, "<=") == 0) return leftVal <= rightVal;
  if (strcmp(op, "==") == 0) return leftVal == rightVal;
  if (strcmp(op, "!=") == 0) return leftVal != rightVal;
  return false;
}

// ---------- Kirim log ke realtime DB agar tampil di Serial Monitor web ----------
void FALectricIoT::_vmSendLog(const char* msg) {
  Serial.println(msg);
  if (!_connected) return;
  // Kirim ke key __PROGRAM_LOG__ agar Web Console bisa membacanya via SSE
  String fk = _deviceId.length() > 0 ? _deviceId + "/__PROGRAM_LOG__" : "__PROGRAM_LOG__";
  String escaped = String(msg);
  escaped.replace("\"", "'");
  String wsMsg = "{\"type\":\"set\",\"key\":\"" + fk + "\",\"value\":\"" + escaped + "\"}";
  _ws.sendTXT(wsMsg);
}
