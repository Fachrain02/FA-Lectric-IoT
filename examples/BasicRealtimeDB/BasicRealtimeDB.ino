/**
 * FA-Lectric-IoT — Basic Realtime Database
 * Kirim data sensor ke FA-LECTRIC IoT Hub secara realtime.
 */

#include <FALectricIoT.h>

void setup() {
  Serial.begin(115200);

  // Connect WiFi + WebSocket (1 baris!)
  fa.begin("WIFI_SSID", "WIFI_PASSWORD", "fal_YOUR_API_KEY");
}

void loop() {
  fa.loop(); // Wajib dipanggil

  // Kirim data ke Realtime Database
  fa.set("Suhu", 27.5);
  fa.set("Kelembaban", 65);
  fa.set("Relay", true);
  fa.set("Status", "Normal");

  // Baca data dari cache (diupdate otomatis dari server)
  float suhu = fa.getFloat("Suhu");
  Serial.println("Suhu: " + String(suhu));

  delay(5000);
}
