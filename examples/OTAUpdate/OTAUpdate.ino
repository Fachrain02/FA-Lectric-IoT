/**
 * FA-Lectric-IoT — Contoh OTA Update (Otomatis aktif)
 *
 * OTA (update program tanpa kabel) sudah AKTIF OTOMATIS di library ini.
 * Cukup pakai fa.begin() + fa.loop() seperti biasa — tidak perlu kode tambahan.
 *
 * Cara update program dari aplikasi:
 *   1. Compile program (Sketch > Export Compiled Binary) -> dapat file .bin
 *   2. Buka aplikasi FA-LECTRIC: menu "Update Program (OTA)"
 *   3. Pilih perangkat, upload file .bin, klik "Update"
 *   4. Perangkat akan mengunduh & memasang program baru, lalu restart.
 *      Progresnya terlihat realtime di aplikasi.
 *
 * Catatan:
 *   - Perangkat harus online (terhubung WiFi & server).
 *   - Gunakan fa.loop() tanpa delay() panjang agar perintah OTA cepat diterima.
 *   - fa.enableOTA(false) bisa dipakai untuk menonaktifkan OTA bila perlu.
 */

#include <FALectricIoT.h>

void setup() {
  Serial.begin(115200);

  // OTA: pakai kredensial WiFi & API Key dari Flash & Setup awal (tersimpan di NVS).
  // Tidak perlu hardcode WiFi — cocok untuk program yang dikirim via OTA.
  fa.begin();

  // Alternatif (hardcode manual):
  // fa.begin("WIFI_SSID", "WIFI_PASSWORD", "fal_xxxxxxxxxxxx");

  // OTA aktif default. Untuk mematikan: fa.enableOTA(false);
}

void loop() {
  fa.loop();  // wajib — handle WebSocket + perintah OTA

  // Contoh kerja normal perangkat
  fa.set("Suhu", 27.5);
  delay(2000);  // delay singkat aman; hindari delay sangat panjang
}
