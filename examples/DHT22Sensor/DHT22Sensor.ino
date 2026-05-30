/**
 * FA-Lectric-IoT — Sensor Suhu & Kelembaban DHT22
 * Kirim data sensor setiap 3 detik tanpa delay().
 * Data langsung muncul di Realtime Database dashboard.
 */

#include <FALectricIoT.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();
  // Cara 1 (manual): isi WiFi & API Key langsung di sini.
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
  // Cara 2 (OTA/ESP32): pakai kredensial dari Flash & Setup awal — tanpa hardcode.
  // fa.begin();
}

void loop() {
  fa.loop();

  if (millis() - lastSend >= 3000) {
    lastSend = millis();

    float suhu = dht.readTemperature();
    float kelembaban = dht.readHumidity();

    if (!isnan(suhu) && !isnan(kelembaban)) {
      fa.set("Suhu", suhu);
      fa.set("Kelembaban", kelembaban);
      fa.set("Status", suhu > 35 ? "PANAS" : "NORMAL");

      Serial.printf("Suhu: %.1f°C | Kel: %.1f%%\n", suhu, kelembaban);
    }
  }
}
