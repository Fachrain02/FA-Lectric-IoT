/**
 * FA-Lectric-IoT — Contoh Folder / Sub-folder (Nested Data)
 *
 * Menyimpan data ke dalam folder & sub-folder di Realtime Database.
 * Cocok untuk banyak titik/ruangan/perangkat dalam satu struktur rapi.
 *
 * Hasil di Realtime Database:
 * {
 *   "Ruangan_1": { "Suhu": 27.5, "Kelembaban": 65, "AC": { "Status": true } },
 *   "Ruangan_2": { "Suhu": 30.1 },
 *   "Mode": "auto"
 * }
 */

#include <FALectricIoT.h>

void setup() {
  Serial.begin(115200);
  fa.begin();  // pakai kredensial dari Flash & Setup awal (atau begin("SSID","PASS","key"))
}

void loop() {
  fa.loop();

  // --- Cara 1: pakai fa.folder() (paling mudah) ---
  fa.folder("Ruangan_1");        // semua set() berikutnya masuk Ruangan_1/
  fa.set("Suhu", 27.5);          // -> Ruangan_1/Suhu
  fa.set("Kelembaban", 65);      // -> Ruangan_1/Kelembaban

  fa.folder("Ruangan_1/AC");     // sub-folder lebih dalam (path lengkap dari root)
  fa.set("Status", true);        // -> Ruangan_1/AC/Status

  fa.folder("");                 // kembali ke root
  fa.set("Mode", "auto");        // -> Mode

  // --- Cara 2: langsung pakai path "/" (tanpa fa.folder) ---
  fa.set("Ruangan_2/Suhu", 30.1);   // -> Ruangan_2/Suhu

  // Membaca data dari folder
  float suhu1 = fa.getFloat("Ruangan_1/Suhu");
  Serial.println("Suhu Ruangan_1: " + String(suhu1));

  delay(5000);
}
