/**
 * FA-Lectric-IoT — Smart Greenhouse
 * Monitoring suhu + kelembaban tanah + kontrol pompa air dari dashboard.
 * Kombinasi: kirim sensor + terima perintah (bidirectional).
 */

#include <FALectricIoT.h>

#define SOIL_PIN 32
#define PUMP_PIN 26

unsigned long lastSend = 0;

// Callback: pompa dikontrol dari dashboard
void onPompa(String value) {
  bool state = (value == "true");
  digitalWrite(PUMP_PIN, state ? HIGH : LOW);
  Serial.println("[FA] Pompa → " + String(state ? "ON" : "OFF"));
}

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);

  // Cara 1 (manual): isi WiFi & API Key langsung di sini.
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
  // Cara 2 (OTA/ESP32): pakai kredensial dari Flash & Setup awal — tanpa hardcode.
  // fa.begin();
  fa.on("Pompa", onPompa);
}

void loop() {
  fa.loop();

  if (millis() - lastSend >= 2000) {
    lastSend = millis();

    // Baca sensor kelembaban tanah (0-4095 → 0-100%)
    int raw = analogRead(SOIL_PIN);
    float kelembaban_tanah = map(raw, 4095, 0, 0, 100);

    // Kirim ke Realtime Database
    fa.set("Kelembaban_Tanah", kelembaban_tanah);
    fa.set("Sensor_Raw", raw);

    // Auto-pompa jika tanah kering (opsional, bisa juga manual dari UI)
    if (kelembaban_tanah < 30 && !fa.getBool("Pompa")) {
      fa.set("Pompa", true);
      digitalWrite(PUMP_PIN, HIGH);
    } else if (kelembaban_tanah > 70 && fa.getBool("Pompa")) {
      fa.set("Pompa", false);
      digitalWrite(PUMP_PIN, LOW);
    }
  }
}
