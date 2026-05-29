/**
 * FA-Lectric-IoT — Subscribe (Terima Perintah dari UI)
 * Callback dipanggil otomatis saat ada perubahan dari dashboard.
 */

#include <FALectricIoT.h>

// Callback: dipanggil saat "Relay" berubah dari UI
void onRelay(String value) {
  Serial.println("Relay diubah ke: " + value);
  if (value == "true") {
    digitalWrite(2, HIGH);
  } else {
    digitalWrite(2, LOW);
  }
}

// Callback: dipanggil saat "Brightness" berubah
void onBrightness(String value) {
  int pwm = value.toInt();
  analogWrite(5, pwm);
  Serial.println("Brightness: " + value);
}

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  fa.begin("WIFI_SSID", "WIFI_PASSWORD", "fal_YOUR_API_KEY");

  // Subscribe ke perubahan key tertentu
  fa.on("Relay", onRelay);
  fa.on("Brightness", onBrightness);
}

void loop() {
  fa.loop(); // Handle WebSocket events + trigger callbacks

  // Tetap bisa kirim data bersamaan tanpa hambatan
  fa.set("Suhu", 27.5);

  delay(2000);
}
