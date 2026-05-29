/**
 * FA-Lectric-IoT — Basic Realtime Database
 * Kirim data sensor ke dashboard secara realtime.
 * Tanpa delay() — pakai millis() agar callback tetap responsif.
 */

#include <FALectricIoT.h>

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
}

void loop() {
  fa.loop();

  // Kirim data setiap 5 detik
  if (millis() - lastSend >= 5000) {
    lastSend = millis();

    fa.set("Suhu", 27.5);
    fa.set("Kelembaban", 65);
    fa.set("Relay", true);
    fa.set("Status", "Normal");

    // Baca dari cache (diupdate otomatis dari server)
    float suhu = fa.getFloat("Suhu");
    Serial.println("Suhu: " + String(suhu));
  }
}
