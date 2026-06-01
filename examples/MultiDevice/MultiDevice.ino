/**
 * FA-Lectric-IoT — Contoh Multi-Device (1 API Key, banyak perangkat)
 *
 * Satu API Key bisa dipakai banyak perangkat (1 proyek = banyak alat).
 * Data tiap perangkat otomatis terpisah di folder bernama deviceId.
 *
 * Flash program yang SAMA ke banyak ESP, cukup ganti deviceId-nya.
 * (atau set deviceId berdasarkan chip ID agar unik otomatis).
 *
 * Hasil di Realtime Database (1 key):
 * {
 *   "sensor-ruang-a": { "Suhu": 27.5, "Kelembaban": 65 },
 *   "sensor-ruang-b": { "Suhu": 30.1, "Kelembaban": 70 }
 * }
 */

#include <FALectricIoT.h>

void setup() {
  Serial.begin(115200);

  // Cara 1: set deviceId lewat parameter ke-4 begin()
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY", "sensor-ruang-a");

  // Cara 2 (OTA, kredensial dari NVS) + set deviceId terpisah:
  // fa.begin();
  // fa.device("sensor-ruang-a");
}

void loop() {
  fa.loop();

  // Semua data otomatis masuk ke folder "sensor-ruang-a/"
  fa.set("Suhu", 27.5);          // -> sensor-ruang-a/Suhu
  fa.set("Kelembaban", 65);      // -> sensor-ruang-a/Kelembaban

  // Tetap bisa pakai sub-folder di dalam device
  fa.folder("sensor-ruang-a/AC");
  fa.set("Status", true);        // -> sensor-ruang-a/AC/Status
  fa.folder("");

  delay(5000);
}
