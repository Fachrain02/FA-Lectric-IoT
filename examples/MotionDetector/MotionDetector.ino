/**
 * FA-Lectric-IoT — Deteksi Gerakan (PIR Sensor)
 * Kirim alert ke dashboard saat ada gerakan terdeteksi.
 * Simpan log kejadian ke Collection.
 */

#include <FALectricIoT.h>

#define PIR_PIN 13
#define LED_PIN 2

bool lastMotion = false;
unsigned long motionCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
}

void loop() {
  fa.loop();

  bool motion = digitalRead(PIR_PIN) == HIGH;

  // Hanya kirim saat status berubah (event-driven, hemat bandwidth)
  if (motion != lastMotion) {
    lastMotion = motion;
    
    fa.set("Gerakan", motion);
    fa.set("Status", motion ? "TERDETEKSI" : "AMAN");
    digitalWrite(LED_PIN, motion ? HIGH : LOW);

    if (motion) {
      motionCount++;
      fa.set("Total_Gerakan", (int)motionCount);

      // Log ke collection setiap ada gerakan
      fa.push("motion_log", "event", 1.0);

      Serial.println("[!] Gerakan terdeteksi! Total: " + String(motionCount));
    } else {
      Serial.println("[OK] Area aman.");
    }
  }
}
