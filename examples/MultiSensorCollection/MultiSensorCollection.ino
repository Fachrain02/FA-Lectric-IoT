/**
 * FA-Lectric-IoT — Multi-Sensor dengan Multiple Collections
 * Kirim data ke Realtime Database + 2 Collection berbeda sekaligus.
 * Contoh: sensor cuaca + sensor tanah → collection terpisah.
 */

#include <FALectricIoT.h>

unsigned long lastWeather = 0;
unsigned long lastSoil = 0;

void setup() {
  Serial.begin(115200);
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
}

void loop() {
  fa.loop();

  // === Sensor Cuaca: setiap 5 detik ===
  if (millis() - lastWeather >= 5000) {
    lastWeather = millis();

    float suhu = 27.0 + random(-10, 10) / 10.0;
    float kelembaban = 65.0 + random(-5, 5);

    // Realtime Database (tampil langsung di dashboard)
    fa.set("Suhu_Udara", suhu);
    fa.set("Kelembaban_Udara", kelembaban);

    // Collection "weather" (histori cuaca)
    fa.push("weather", "suhu", suhu, "kelembaban", kelembaban);
  }

  // === Sensor Tanah: setiap 10 detik ===
  if (millis() - lastSoil >= 10000) {
    lastSoil = millis();

    float moisture = random(20, 80);
    float ph = 6.0 + random(-10, 10) / 10.0;

    // Realtime Database
    fa.set("Kelembaban_Tanah", moisture);
    fa.set("pH_Tanah", ph);

    // Collection "soil" (histori tanah — terpisah dari cuaca)
    fa.push("soil", "moisture", moisture, "ph", ph);
  }
}
