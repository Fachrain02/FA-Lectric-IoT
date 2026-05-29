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

## License

MIT License - (c) 2026 Fachrain Azis, FA-LECTRIC
