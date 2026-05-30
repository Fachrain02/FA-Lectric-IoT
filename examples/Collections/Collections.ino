/**
 * FA-Lectric-IoT — Collections (SQL-Style Storage)
 * Simpan data sensor sebagai record dengan ID & timestamp otomatis.
 * Tanpa delay() — pakai millis().
 */

#include <FALectricIoT.h>

unsigned long lastPush = 0;

void setup() {
  Serial.begin(115200);
  // Cara 1 (manual): isi WiFi & API Key langsung di sini.
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
  // Cara 2 (OTA/ESP32): pakai kredensial dari Flash & Setup awal — tanpa hardcode.
  // fa.begin();
}

void loop() {
  fa.loop();

  if (millis() - lastPush >= 5000) {
    lastPush = millis();

    // Push 2 fields ke collection "sensor"
    fa.push("sensor",
      "suhu", 27.5,
      "kelembaban", 65
    );

    // Push 3 fields ke collection "sensor"
    fa.push("sensor",
      "suhu", 28.1,
      "kelembaban", 63,
      "tekanan", 1013.25
    );

    Serial.println("[FA] Data pushed to collection.");
  }
}
