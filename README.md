# ESP32 DevKit V1 — Cloud Gateway Node

This directory contains the firmware for the ESP32, which acts as the
system's Internet gateway (`esp32_station_1`). It is the only device in the
system that talks to Wi-Fi and the cloud.

## What this node does

- Connects to Wi-Fi with automatic reconnection on drop
- Receives JSON sensor data from the Arduino Mega over UART2
- Optionally reads its own local DHT11/MQ-135 sensors
- Uploads aggregated readings from both nodes to a single ThingSpeak channel
- Buffers up to `OFFLINE_BUFFER_MAX_ENTRIES` readings in RAM if the cloud is
  temporarily unreachable, and flushes them once connectivity returns
- Marks the Mega node OFFLINE if no JSON has arrived within the configured timeout
- Synchronizes time via NTP for local diagnostics (Africa/Lusaka, UTC+2)

## Wiring summary

| Component            | ESP32 Pin | Notes                                              |
|-----------------------|-----------|-----------------------------------------------------|
| DHT11 DATA (optional)  | GPIO4     | Only if a local sensor is installed                |
| MQ-135 AO (optional)   | GPIO34    | ADC1-capable, input-only pin                        |
| Light sensor (optional)| GPIO35    | ADC1-capable, input-only pin; shows N/A if unused   |
| Mega link RX (UART2)   | GPIO16    | Receives from Mega TX2, **through a level shifter** |
| Mega link TX (UART2)   | GPIO17    | Sends to Mega RX2                                   |

See `../docs/hardware-connections.md` for the full wiring diagram and the
mandatory voltage-level-shifting requirement between the Mega's 5V logic and
the ESP32's 3.3V logic.

## Setup

1. Install the ESP32 board package in the Arduino IDE (Boards Manager:
   search "esp32" by Espressif Systems).
2. Install these libraries via Library Manager:
   - `ThingSpeak` (by MathWorks)
   - `ArduinoJson` (by Benoit Blanchon)
   - `DHT sensor library` (by Adafruit) + `Adafruit Unified Sensor`
3. Copy `config.example.h` to `config.h` (already done in this template —
   re-copy if you reset the project).
4. Edit `config.h`:
   - `WIFI_SSID` / `WIFI_PASSWORD`
   - `THINGSPEAK_WRITE_API_KEY` (never share this key publicly)
   - `ESP32_HAS_LOCAL_SENSORS` — set to `0` if no sensors are wired directly
     to the ESP32
5. Select **ESP32 Dev Module** as the board, choose the correct COM port.
6. Upload.
7. Open the Serial Monitor at 115200 baud and confirm:
   - `WiFi connected`
   - `ThingSpeak: CONNECTED`
   - `Mega: ONLINE` (once the Mega is powered and wired correctly)

## ThingSpeak field mapping

| Field | Meaning                                    |
|-------|---------------------------------------------|
| 1     | Mega Temperature (°C)                        |
| 2     | Mega Humidity (%)                            |
| 3     | Mega Air Quality (raw MQ-135 ADC)            |
| 4     | ESP32 Temperature (°C)                       |
| 5     | ESP32 Humidity (%)                           |
| 6     | ESP32 Air Quality (raw MQ-135 ADC)           |
| 7     | Node/health status bitmask (see firmware comments) |
| 8     | Alert level (0 = none, 1 = warning, 2 = critical) |

This mapping is also documented in `../docs/architecture.md` and matches
what `dashboard/app.js` expects when parsing ThingSpeak responses.
