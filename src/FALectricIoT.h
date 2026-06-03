
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
#define FA_HEARTBEAT_INTERVAL 15000
#define FA_JSON_BUFFER_SIZE 1024

// Zero-OTA Dynamic VM Engine
#define FA_VM_MAX_VARS     20
#define FA_VM_MAX_INSTR    80
#define FA_VM_WATCHDOG_MS  8000

struct FAVMVar {
  char name[24];
  float value;
};

struct FAVMProgram {
  bool loaded;
  bool running;
  bool setupDone;
  String setupJson;
  String loopJson;
  FAVMVar vars[FA_VM_MAX_VARS];
  uint8_t varCount;
};

typedef void (*FACallback)(String value);

struct FASubscription {
  String key;
  FACallback callback;
};

class FALectricIoT {
public:
  FALectricIoT();

  void begin();
  void begin(const char* ssid, const char* password, const char* deviceKey);
  void begin(const char* ssid, const char* password, const char* deviceKey, const char* deviceId);
  void begin(const char* ssid, const char* password, const char* deviceKey, const char* host, uint16_t port);
  void loop();
  bool connected();

  void device(const char* deviceId);

  void setServer(const char* host, uint16_t port);

  void set(const char* key, float value);
  void set(const char* key, double value);
  void set(const char* key, int value);
  void set(const char* key, bool value);
  void set(const char* key, const char* value);
  void set(const char* key, String value);

  void folder(const char* path);

  float getFloat(const char* key);
  int getInt(const char* key);
  bool getBool(const char* key);
  String getString(const char* key);

  void push(const char* collection, const char* key1, float val1);
  void push(const char* collection, const char* key1, float val1, const char* key2, float val2);
  void push(const char* collection, const char* key1, float val1, const char* key2, float val2, const char* key3, float val3);
  void push(const char* collection, const char* key1, float val1, const char* key2, float val2, const char* key3, float val3, const char* key4, float val4);
  void pushJSON(const char* collection, const char* jsonPayload);

  void on(const char* key, FACallback callback);

  void enableOTA(bool enable);

  #ifdef ESP32
  static String encryptAES(String plaintext, const char* key32);
  static String decryptAES(String ciphertext, const char* key32);
  #endif

private:
  WebSocketsClient _ws;
  const char* _deviceKey;
  const char* _host;
  uint16_t _port;
  bool _connected;
  unsigned long _lastReconnect;
  unsigned long _lastHeartbeat;
  bool _otaEnabled;

  String _nvsSsid;
  String _nvsPass;
  String _nvsKey;
  String _nvsHost;
  String _nvsPubKey;
  String _otaSignature;

  String _currentFolder;

  String _deviceId;

  StaticJsonDocument<FA_JSON_BUFFER_SIZE> _cache;

  FASubscription _subs[FA_MAX_SUBSCRIPTIONS];
  uint8_t _subCount;

  void _connectWiFi(const char* ssid, const char* password);
  String _fullKey(const char* key);
  void _connectWebSocket();
  void _handleMessage(uint8_t* payload, size_t length);
  void _sendSet(const char* key, const char* value);
  void _notifySubscribers(const char* key, const char* value);
  void _performOTA(const char* url, const char* version);
  void _reportOTA(const char* phase, int percent, const char* message);
  static void _wsEvent(WStype_t type, uint8_t* payload, size_t length);
  bool _verifySignature(const uint8_t* hash, size_t hashLen, const String& signatureBase64, const String& pubKeyPEM);

  // Zero-OTA VM
  FAVMProgram _vm;
  void _vmLoadProgram(const char* setupJson, const char* loopJson, const char* varsJson);
  void _vmRunBlock(const char* jsonArray);
  void _vmExecInstr(JsonArray& instr);
  void _vmSetVar(const char* name, float val);
  float _vmGetVar(const char* name);
  void _vmSendLog(const char* msg);
  bool _vmEvalCondition(const char* leftName, const char* op, float rightVal);
};

extern FALectricIoT fa;

#endif
