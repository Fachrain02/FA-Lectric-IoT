/**
 * FA-Lectric-IoT — Collections (SQL-Style Storage)
 * Simpan data sensor sebagai record dengan ID & timestamp otomatis.
 */

#include <FALectricIoT.h>

void setup() {
  Serial.begin(115200);
  fa.begin("WIFI_SSID", "WIFI_PASSWORD", "fal_YOUR_API_KEY");
}

void loop() {
  fa.loop();

  // Push record ke collection "sensor" (otomatis dibuat jika belum ada)
  fa.push("sensor",
    "suhu", 27.5,
    "kelembaban", 65
  );

  // Push dengan lebih banyak field
  fa.push("sensor",
    "suhu", 28.1,
    "kelembaban", 63,
    "tekanan", 1013.25
  );

  delay(5000);
}
