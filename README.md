# FA-Lectric-IoT

Library Arduino untuk menghubungkan ESP32/ESP8266 ke **FA-LECTRIC IoT Hub** secara realtime via WebSocket.

## Fitur

- **Koneksi Persistent** — WebSocket selalu terbuka, tanpa buka-tutup koneksi
- **Zero Latency** — Data langsung sampai tanpa jeda
- **Bidirectional** — Kirim dan terima data di koneksi yang sama
- **Realtime Database** — Set/Get data seperti Firebase
- **Collections** — Simpan data terstruktur dengan ID & timestamp otomatis
- **Subscribe** — Terima callback saat data berubah dari UI/device lain
- **Auto-Reconnect** — Koneksi otomatis pulih jika terputus
- **Super Simpel** — Cukup 3 baris untuk mulai

## Board yang Didukung

- ESP32 (semua varian: DevKit, S2, S3, C3)
- ESP8266 (NodeMCU, Wemos D1 Mini, dll)

## Instalasi

### Arduino IDE
1. Download ZIP dari [Releases](https://github.com/Fachrain02/FA-Lectric-IoT/releases)
2. Arduino IDE → Sketch → Include Library → Add .ZIP Library

### PlatformIO
```ini
lib_deps = FA-Lectric-IoT
```

## Quick Start

```cpp
#include <FALectricIoT.h>

void setup() {
  Serial.begin(115200);
  fa.begin("WIFI_SSID", "WIFI_PASS", "fal_YOUR_API_KEY");
}

void loop() {
  fa.loop();
  fa.set("Suhu", 27.5);
  delay(5000);
}
```

## API Reference

### Setup
| Method | Keterangan |
|--------|-----------|
| `fa.begin(ssid, pass, key)` | Connect WiFi + server |
| `fa.begin(ssid, pass, key, host, port)` | Custom server |
| `fa.loop()` | Wajib di `loop()` |
| `fa.connected()` | Cek status koneksi |

### Realtime Database
| Method | Keterangan |
|--------|-----------|
| `fa.set("key", value)` | Kirim data (float/int/bool/string) |
| `fa.getFloat("key")` | Baca float dari cache |
| `fa.getInt("key")` | Baca int dari cache |
| `fa.getBool("key")` | Baca bool dari cache |
| `fa.getString("key")` | Baca string dari cache |

### Collections
| Method | Keterangan |
|--------|-----------|
| `fa.push("col", "k1", v1, "k2", v2, ...)` | Push record (max 4 fields) |
| `fa.pushJSON("col", "{...}")` | Push raw JSON |

### Subscribe
| Method | Keterangan |
|--------|-----------|
| `fa.on("key", callback)` | Subscribe perubahan key |

## Dependencies

- [WebSockets](https://github.com/Links2004/arduinoWebSockets) by Markus Sattler
- [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon

## Keamanan & Privasi (Secure OTA)

Library ini dilengkapi dengan fitur **Exclusive Secure OTA** berbasis tanda tangan digital (**ECDSA P-256**) untuk memastikan update program hanya bisa dilakukan melalui dashboard resmi aplikasi Anda secara aman:

1. **Cryptographic Signing (ECDSA P-256 & SHA-256)**:
   - Setiap file biner program (.bin) di-hash menggunakan **SHA-256** dan ditandatangani secara dinamis di server Next.js menggunakan **Private Key** unik milik perangkat.
   - ESP32 mengunduh file biner secara *streaming* dan memverifikasi tanda tangannya secara langsung di memori menggunakan pustaka bawaan `mbedtls` sebelum menulisnya ke partisi program.

2. **Zero Hardcoded Credentials**:
   - **TIDAK ada** kunci rahasia (*Private Key*), kunci publik (*Public Key*), kredensial database, atau token API yang tertanam (hardcoded) di dalam kode pustaka (Library) maupun kode contoh (*Examples*).
   - *Public Key* di-generate secara otomatis dan diinjeksikan secara aman dari peramban (browser) ke memori **NVS (Non-Volatile Storage / Preferences)** ESP32 secara lokal saat proses **Flash & Setup** via Web Serial API.

3. **Kebebasan Perangkat (Physical Ownership)**:
   - Pengamanan ini hanya mengunci jalur update udara (OTA) agar tidak bisa disusupi oleh pihak lain.
   - Pengguna tetap memegang kendali fisik penuh atas ESP32 mereka. Board tetap dapat dipakai untuk proyek lain di luar ekosistem aplikasi ini dengan cara mem-flash program baru secara langsung menggunakan kabel USB.

## License

MIT License - (c) 2026 Fachrain Azis, FA-LECTRIC
