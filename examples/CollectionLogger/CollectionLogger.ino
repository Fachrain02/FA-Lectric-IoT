/**
 * FA-Lectric-IoT — Data Logger ke Collection
 * Simpan pembacaan sensor secara berkala ke Collection (SQL-style).
 * Data tersimpan dengan ID unik + timestamp otomatis.
 * Cocok untuk grafik historis & analisis data.
 */

#include <FALectricIoT.h>

unsigned long lastPush = 0;
int recordCount = 0;

void setup() {
  Serial.begin(115200);
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
}

void loop() {
  fa.loop();

  // Push record setiap 10 detik
  if (millis() - lastPush >= 10000) {
    lastPush = millis();
    recordCount++;

    // Simulasi data sensor
    float suhu = 25.0 + random(-20, 20) / 10.0;
    float kelembaban = 60.0 + random(-10, 10);
    float tekanan = 1013.0 + random(-5, 5) / 10.0;

    // Push ke collection "environment" (auto-create jika belum ada)
    fa.push("environment",
      "suhu", suhu,
      "kelembaban", kelembaban,
      "tekanan", tekanan,
      "record", (float)recordCount
    );

    Serial.printf("[FA] Record #%d pushed: T=%.1f H=%.1f P=%.1f\n",
      recordCount, suhu, kelembaban, tekanan);
  }
}
