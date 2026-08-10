/*
 * config.example.h
 * -----------------------------------------------------------------------
 * Template configuration file for the ESP32 Environment Gateway.
 *
 * HOW TO USE:
 *   1. Copy this file and rename the copy to "config.h"
 *   2. Fill in your real Wi-Fi credentials and ThingSpeak Write API Key.
 *   3. config.h is listed in .gitignore and must NEVER be committed to a
 *      public repository.
 *
 * This file (config.example.h) contains placeholder values only and is
 * safe to commit.
 * -----------------------------------------------------------------------
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// NODE IDENTITY
// ============================================================
#define NODE_ID              2
#define NODE_NAME            "esp32_station_1"
#define NODE_DISPLAY_NAME    "ESP32 Station 1"

// ============================================================
// WIFI CREDENTIALS  (fill these in -- never commit real values)
// ============================================================
#define WIFI_SSID             "YOUR_WIFI_NAME"
#define WIFI_PASSWORD         "YOUR_WIFI_PASSWORD"
#define WIFI_CONNECT_TIMEOUT_MS   20000UL
#define WIFI_RETRY_INTERVAL_MS    10000UL

// ============================================================
// THINGSPEAK CONFIGURATION
// ============================================================
// Channel: CBU Environmental Monitoring System (ID 3445381)
// IMPORTANT: this is the WRITE key only. It must never appear in the
// dashboard/frontend code. Regenerate before production use if this
// key has ever been shared or committed anywhere previously.
#define THINGSPEAK_CHANNEL_ID     3445381UL
#define THINGSPEAK_WRITE_API_KEY  "YOUR_WRITE_API_KEY"
#define THINGSPEAK_SERVER         "api.thingspeak.com"

// ThingSpeak free tier allows updates no faster than every 15 seconds.
// We default to a safer 20 seconds, configurable here.
#define THINGSPEAK_INTERVAL_MS    20000UL

// ============================================================
// ESP32 LOCAL SENSOR PINS
// ============================================================
// Set ESP32_HAS_LOCAL_SENSORS to 0 if no sensors are physically wired
// to the ESP32 (Mega-only deployment). Set to 1 if the ESP32 also has
// its own DHT11 / MQ-135 attached.
#define ESP32_HAS_LOCAL_SENSORS   1

#define ESP32_DHT_PIN             4      // GPIO4, safe general-purpose pin
#define ESP32_DHTTYPE             DHT11
#define ESP32_MQ135_PIN           34     // GPIO34, ADC1-capable, input-only

// Light sensor: set to 1 only if a physical LDR/photoresistor or light
// sensor module is wired to the ESP32. If 0, dashboard displays "N/A".
#define ESP32_HAS_LIGHT_SENSOR    0
#define ESP32_LIGHT_PIN           35     // GPIO35, ADC1-capable, input-only (if used)

// ============================================================
// MEGA <-> ESP32 UART LINK
// ============================================================
// ESP32 Serial2 (default pins: RX2=16, TX2=17) is used for the link to
// the Arduino Mega. Confirm these pins are free on your specific board
// wiring before use.
#define MEGA_LINK_RX_PIN          16
#define MEGA_LINK_TX_PIN          17
#define MEGA_LINK_BAUD_RATE       115200

// If no JSON has been received from the Mega within this window, the
// dashboard/ThingSpeak status field marks the Mega node OFFLINE.
#define MEGA_OFFLINE_TIMEOUT_MS   30000UL

// ============================================================
// OFFLINE BUFFERING
// ============================================================
#define OFFLINE_BUFFER_MAX_ENTRIES  20

// ============================================================
// TIME / NTP
// ============================================================
#define NTP_SERVER            "pool.ntp.org"
// Africa/Lusaka is UTC+2, no daylight saving.
#define GMT_OFFSET_SEC         (2 * 3600)
#define DAYLIGHT_OFFSET_SEC    0

// ============================================================
// DEBUG MODE
// ============================================================
#define DEBUG_MODE 1

#endif // CONFIG_H
