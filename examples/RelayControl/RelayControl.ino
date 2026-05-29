/**
 * FA-Lectric-IoT — Kontrol Relay dari Dashboard
 * Relay menyala/mati otomatis saat user klik toggle di UI.
 * Tanpa delay(), realtime response.
 */

#include <FALectricIoT.h>

#define RELAY_PIN 2

// Callback: dipanggil otomatis saat "Relay" berubah dari dashboard
void onRelay(String value) {
  bool state = (value == "true");
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  Serial.println("[FA] Relay → " + value);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);

  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
  fa.on("Relay", onRelay);
}

void loop() {
  fa.loop();
  // Tidak perlu kode lain — relay dikontrol sepenuhnya dari dashboard
}
