/**
 * FA-Lectric-IoT Library
 * Realtime IoT communication via WebSocket for ESP32 & ESP8266
 * 
 * https://fa-lectric.com
 * https://github.com/Fachrain02/FA-Lectric-IoT
 * 
 * (c) 2026 Fachrain Azis - FA-LECTRIC
 */

#ifndef FA_LECTRIC_IOT_H
#define FA_LECTRIC_IOT_H

#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef ESP32
  #include <WiFi.h>
  #include <WebSocketsClient.h>
  #include <HTTPClient.h>
  #include <Update.h>
  #include <Preferences.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WebSocketsClient.h>
  #include <ESP8266HTTPClient.h>
  #include <Updater.h>
  #include <EEPROM.h>
#else
  #error "FA-Lectric-IoT only supports ESP32 and ESP8266"
#endif

#define FA_MAX_SUBSCRIPTIONS 20
#define FA_RECONNECT_INTERVAL 3000
#define FA_JSON_BUFFER_SIZE 1024

typedef void (*FACallback)(String value);

struct FASubscription {
  String key;
  FACallback callback;
};

class FALectricIoT {
public:
  FALectricIoT();

  // Setup & Connection
  void begin();  // Baca SSID/password/key dari NVS (hasil setup awal via OTA_Setup)
  void begin(const char* ssid, const char* password, const char* deviceKey);
  void begin(const char* ssid, const char* password, const char* deviceKey, const char* host, uint16_t port);
  void loop();
  bool connected();

  // Server config (optional — defaults to fa-lectric.com:80 via NGINX /ws)
  void setServer(const char* host, uint16_t port);

  // Realtime Database — Write
  void set(const char* key, float value);
  void set(const char* key, int value);
  void set(const char* key, bool value);
  void set(const char* key, const char* value);
  void set(const char* key, String value);

  // Realtime Database — Read (from local cache, updated via WebSocket)
  float getFloat(const char* key);
  int getInt(const char* key);
  bool getBool(const char* key);
  String getString(const char* key);

  // Collections — Push record
  void push(const char* collection, const char* key1, float val1);
  void push(const char* collection, const char* key1, float val1, const char* key2, float val2);
  void push(const char* collection, const char* key1, float val1, const char* key2, float val2, const char* key3, float val3);
  void push(const char* collection, const char* key1, float val1, const char* key2, float val2, const char* key3, float val3, const char* key4, float val4);
  void pushJSON(const char* collection, const char* jsonPayload);

  // Subscribe — Callback when key changes (from UI or other devices)
  void on(const char* key, FACallback callback);

  // OTA — Update program via website (dipicu manual oleh user dari aplikasi).
  // Aktif otomatis: dengarkan perintah OTA melalui WebSocket di fa.loop().
  // Tidak ada polling — nol beban saat idle.
  void enableOTA(bool enable);   // default: aktif

private:
  WebSocketsClient _ws;
  const char* _deviceKey;
  const char* _host;
  uint16_t _port;
  bool _connected;
  unsigned long _lastReconnect;
  bool _otaEnabled;

  // Penyimpanan kredensial saat dibaca dari NVS (agar pointer tetap valid)
  String _nvsSsid;
  String _nvsPass;
  String _nvsKey;
  String _nvsHost;

  // Local data cache
  StaticJsonDocument<FA_JSON_BUFFER_SIZE> _cache;

  // Subscriptions
  FASubscription _subs[FA_MAX_SUBSCRIPTIONS];
  uint8_t _subCount;

  // Internal
  void _connectWiFi(const char* ssid, const char* password);
  void _connectWebSocket();
  void _handleMessage(uint8_t* payload, size_t length);
  void _sendSet(const char* key, const char* value);
  void _notifySubscribers(const char* key, const char* value);
  void _performOTA(const char* url, const char* version);
  void _reportOTA(const char* phase, int percent, const char* message);
  static void _wsEvent(WStype_t type, uint8_t* payload, size_t length);
};

// Global instance — user just uses `fa.begin(...)` etc.
extern FALectricIoT fa;

#endif
