# Arduino Mega 2560 — Sensor Node

This directory contains the firmware for the Arduino Mega 2560, which acts as
the primary environmental sensor node (`mega_station_1`) and SMS alert source.

## What this node does

- Reads temperature and humidity from a DHT11 sensor
- Reads raw air-quality ADC values from an MQ-135 sensor
- Sends SMS alerts through a SIM800L GSM module when thresholds are exceeded
- Streams structured JSON sensor data to the ESP32 gateway over a dedicated UART link
- Has **no Wi-Fi hardware** — it never talks to the Internet directly

## Wiring summary

| Component     | Mega Pin                | Notes                                   |
|---------------|--------------------------|------------------------------------------|
| DHT11 DATA    | D7                        | 5V logic, matches Mega natively         |
| MQ-135 AO     | A0                        | Analog read, raw ADC value only         |
| SIM800L RX    | TX1 / pin 18              | Use appropriate current-capable 4V power|
| SIM800L TX    | RX1 / pin 19              | |
| ESP32 link TX | TX2 / pin 16              | **Must be level-shifted to 3.3V** before reaching ESP32 RX |
| ESP32 link RX | RX2 / pin 17              | |

See `../docs/hardware-connections.md` for the full wiring diagram, including
the required voltage-level shifting between the Mega (5V logic) and the ESP32
(3.3V logic).

## Setup

1. Open `mega_environment_monitor/mega_environment_monitor.ino` in the Arduino IDE.
2. Install the **DHT sensor library** by Adafruit (and its dependency,
   **Adafruit Unified Sensor**) via Library Manager.
3. Copy `config.example.h` to `config.h` inside the same folder (this has
   already been done for you in this template, but re-copy it if you ever
   reset the project).
4. Edit `config.h`:
   - Set `SMS_DESTINATION_NUMBER` to the phone number that should receive alerts.
   - Adjust `TEMP_WARNING_THRESHOLD` and `AIR_QUALITY_WARNING_THRESHOLD` if needed.
5. Select **Arduino Mega or Mega 2560** as the board, choose the correct COM port.
6. Upload.
7. Open the Serial Monitor at 115200 baud to confirm sensor readings and
   JSON output.

## Notes on the MQ-135 reading

The MQ-135 analog output is an **uncalibrated raw ADC value**, not a
scientifically calibrated ppm gas concentration. See
`../docs/architecture.md` and the dashboard's own disclaimer text for details
on what would be required for true calibration.
