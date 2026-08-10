# Distributed IoT-Based Environmental Monitoring and Alert System

A cloud-first, laptop-independent IoT system that monitors temperature,
humidity, and air quality using an Arduino Mega sensor node and an ESP32
Wi-Fi gateway, stores data on ThingSpeak, and presents it through a
professional, responsive, static web dashboard.

```text
Arduino Mega ──Serial──▶ ESP32 DevKit V1 ──Wi-Fi──▶ ThingSpeak Cloud ──HTTPS──▶ Web Dashboard ──▶ Laptop / Phone / Tablet
```

## Project overview

This project demonstrates a distributed sensing architecture: a
Wi-Fi-less Arduino Mega handles sensing and SMS alerting, while an ESP32
acts as both a second sensor node and the Internet gateway that relays
everything to the cloud. Once powered and connected, the system requires
**no laptop, no local server, no Node.js, and no local database** to
operate — the dashboard is a static site that talks directly to
ThingSpeak's public REST API.

## Objectives

- Continuously monitor temperature, humidity, and air quality
- Store and visualize historical environmental data in the cloud
- Alert via SMS when configurable thresholds are exceeded
- Provide a professional, responsive dashboard accessible from anywhere
- Support future expansion to additional sensor nodes
- Operate independently of any single laptop or local server

## Features

- Distributed two-node sensing (Arduino Mega + ESP32)
- Automatic Wi-Fi reconnection and cloud-upload retry with offline buffering
- SMS alerting via SIM800L with configurable thresholds and cooldown
- Live dashboard with summary cards, device status, historical charts,
  multi-node comparison, and a derived alert feed
- Dark/light mode, responsive layout (desktop/tablet/mobile), CSV export,
  print-friendly report view
- Honest handling of missing/uncalibrated data: `N/A` instead of fake
  readings, and clear MQ-135 calibration disclosure

## Hardware requirements

- Arduino Mega 2560
- ESP32 DevKit V1
- DHT11 (×1 required on Mega, ×1 optional on ESP32)
- MQ-135 (×1 required on Mega, ×1 optional on ESP32)
- SIM800L GSM module + appropriate external power supply
- Logic-level shifter (Mega 5V ↔ ESP32 3.3V)

See `docs/hardware-connections.md` for full wiring details and required
safety precautions.

## Software requirements

- Arduino IDE (for flashing firmware)
- Arduino libraries: `DHT sensor library` + `Adafruit Unified Sensor`,
  `ThingSpeak`, `ArduinoJson`
- ESP32 board package (Espressif) installed in Arduino IDE
- A modern web browser (for the dashboard)
- A ThingSpeak account and channel
- A static hosting provider for the dashboard (GitHub Pages recommended)

## Project structure

```text
distributed-iot-environment-monitor/
├── README.md
├── docs/
│   ├── architecture.md
│   ├── hardware-connections.md
│   ├── deployment.md
│   ├── troubleshooting.md
│   └── project-presentation.md
├── arduino-mega/
│   ├── mega_environment_monitor/
│   │   ├── mega_environment_monitor.ino
│   │   ├── config.example.h
│   │   └── config.h
│   └── README.md
├── esp32/
│   ├── esp32_environment_gateway/
│   │   ├── esp32_environment_gateway.ino
│   │   ├── config.example.h
│   │   └── config.h
│   └── README.md
├── dashboard/
│   ├── index.html
│   ├── style.css
│   ├── app.js
│   ├── config.js
│   └── assets/
├── deployment/
│   ├── github-pages.md
│   └── thingspeak.md
└── .gitignore
```

## Quick start

1. **Hardware wiring** — follow `docs/hardware-connections.md`, paying
   close attention to the mandatory voltage-level shifting between the
   Mega and ESP32.
2. **Arduino Mega firmware** — see `arduino-mega/README.md`.
3. **ESP32 firmware** — see `esp32/README.md`.
4. **ThingSpeak setup** — see `deployment/thingspeak.md`.
5. **Dashboard configuration and deployment** — see
   `docs/deployment.md` and `deployment/github-pages.md`.

Full step-by-step instructions live in `docs/deployment.md`.

## Security

- Wi-Fi credentials and the ThingSpeak **Write** API key live only in
  firmware `config.h` files, which are excluded from version control by
  `.gitignore`.
- The dashboard only ever uses the ThingSpeak **Read** API key.
- The API keys referenced anywhere in this project's documentation were
  used during development and **should be regenerated** before any
  production deployment — see `deployment/thingspeak.md`.

## Testing checklist

See the bottom of `docs/deployment.md` and `docs/troubleshooting.md` for
diagnostic steps. At minimum, verify:

- [ ] DHT11 and MQ-135 readings on both nodes
- [ ] Mega → ESP32 serial communication
- [ ] ESP32 → Wi-Fi → ThingSpeak upload
- [ ] Dashboard read of latest + historical ThingSpeak data
- [ ] Wi-Fi disconnect / reconnect behavior
- [ ] ThingSpeak unreachable → offline buffering → flush on reconnect
- [ ] Mega disconnected → ESP32 continues, dashboard shows `OFFLINE`
- [ ] SMS alert firing + cooldown behavior
- [ ] Dashboard on desktop, tablet, and mobile, in both themes

## Future improvements

- Additional ESP32 sensor nodes (`esp32_station_2`, `esp32_station_3`, …)
- Flash-backed (not just RAM) offline buffering on the ESP32
- Proper MQ-135 calibration curve for real ppm estimates
- Optional MQTT path as an alternative to ThingSpeak's REST API

## License

Provided as-is for educational/university project use. Review and adapt
security practices (API key handling, SMS destination numbers) before any
production or public deployment.
