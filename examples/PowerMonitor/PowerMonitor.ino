/**
 * FA-Lectric-IoT — Monitor Tegangan & Arus (Sensor ACS712 + Voltage Divider)
 * Kirim data daya listrik ke Realtime Database + simpan histori ke Collection.
 */

#include <FALectricIoT.h>

#define VOLTAGE_PIN 34
#define CURRENT_PIN 35

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  // Cara 1 (manual): isi WiFi & API Key langsung di sini.
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
  // Cara 2 (OTA/ESP32): pakai kredensial dari Flash & Setup awal — tanpa hardcode.
  // fa.begin();
}

void loop() {
  fa.loop();

  if (millis() - lastSend >= 5000) {
    lastSend = millis();

    // Baca sensor
    float tegangan = analogRead(VOLTAGE_PIN) * (3.3 / 4095.0) * 5.0; // Voltage divider ratio
    float arus = (analogRead(CURRENT_PIN) - 2048) * (3.3 / 4095.0) / 0.066; // ACS712 30A
    float daya = tegangan * arus;

    // Kirim ke Realtime Database (tampil langsung di dashboard)
    fa.set("Tegangan", tegangan);
    fa.set("Arus", arus);
    fa.set("Daya", daya);

    // Simpan ke Collection (histori, auto-rotate)
    fa.push("power_log",
      "tegangan", tegangan,
      "arus", arus,
      "daya", daya
    );

    Serial.printf("V=%.2f A=%.2f W=%.2f\n", tegangan, arus, daya);
  }
}
