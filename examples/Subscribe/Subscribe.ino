/**
 * FA-Lectric-IoT — Subscribe (Terima Perintah dari Dashboard)
 * Callback dipanggil otomatis saat ada perubahan dari UI.
 * Bisa kontrol relay, LED, motor, dll dari jarak jauh.
 */

#include <FALectricIoT.h>

#define RELAY_PIN 2
#define LED_PIN 4

unsigned long lastSend = 0;

// Callback: Relay diubah dari dashboard
void onRelay(String value) {
  bool state = (value == "true");
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  Serial.println("[FA] Relay → " + value);
}

// Callback: Brightness diubah dari dashboard (PWM)
void onBrightness(String value) {
  int pwm = value.toInt();
  analogWrite(LED_PIN, pwm);
  Serial.println("[FA] Brightness → " + value);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");

  // Daftarkan callback (sekali saja di setup)
  fa.on("Relay", onRelay);
  fa.on("Brightness", onBrightness);
}

void loop() {
  fa.loop(); // Handle WebSocket + trigger callbacks

  // Tetap bisa kirim data sensor bersamaan (tidak mengganggu subscribe)
  if (millis() - lastSend >= 3000) {
    lastSend = millis();
    fa.set("Suhu", 27.5);
    fa.set("Uptime", (int)(millis() / 1000));
  }
}
