/*
 * =============================================================================
 *  Distributed IoT Environmental Monitoring and Alert System
 *  ESP32 DevKit V1 - Cloud Gateway Firmware
 * =============================================================================
 *
 *  Role in the system:
 *    - Connects to Wi-Fi and keeps the connection alive (auto-reconnect)
 *    - Receives JSON sensor data from the Arduino Mega over UART (Serial2)
 *    - Reads its own local sensors (DHT11 / MQ-135), if physically present
 *    - Aggregates both nodes' data
 *    - Uploads all fields to a single ThingSpeak channel update
 *    - Buffers a limited number of readings in memory if Wi-Fi/ThingSpeak
 *      is temporarily unavailable, and flushes the buffer once back online
 *    - Detects Mega-offline condition, sensor failures, and cloud failures
 *    - Synchronizes time via NTP for local timestamping/logging
 *
 *  ThingSpeak field mapping (see docs/architecture.md for rationale):
 *    Field 1 = Mega Temperature (C)
 *    Field 2 = Mega Humidity (%)
 *    Field 3 = Mega Air Quality (raw MQ-135 ADC)
 *    Field 4 = ESP32 Temperature (C)
 *    Field 5 = ESP32 Humidity (%)
 *    Field 6 = ESP32 Air Quality (raw MQ-135 ADC)
 *    Field 7 = Node/Health status code (see buildStatusCode())
 *    Field 8 = Alert status code (0=none, 1=warning, 2=critical)
 *
 *  Required libraries (Arduino Library Manager):
 *    - WiFi (bundled with ESP32 board package)
 *    - HTTPClient (bundled with ESP32 board package)
 *    - ThingSpeak (by MathWorks)
 *    - ArduinoJson (by Benoit Blanchon)
 *    - DHT sensor library (by Adafruit) + Adafruit Unified Sensor
 * =============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ThingSpeak.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>
#include "config.h"

// -----------------------------------------------------------------------
// GLOBAL OBJECTS
// -----------------------------------------------------------------------
WiFiClient client;

#if ESP32_HAS_LOCAL_SENSORS
DHT dht(ESP32_DHT_PIN, ESP32_DHTTYPE);
#endif

// HardwareSerial for the Mega link (ESP32 has 3 UARTs; UART2 used here)
HardwareSerial MegaLink(2);

// -----------------------------------------------------------------------
// DATA STRUCTURES
// -----------------------------------------------------------------------
struct SensorReading {
  bool  valid       = false;
  float temperature = NAN;
  float humidity    = NAN;
  int   airQuality  = 0;
};

struct BufferedUpdate {
  unsigned long timestamp;
  SensorReading megaData;
  SensorReading esp32Data;
  bool megaOnline;
};

// -----------------------------------------------------------------------
// STATE
// -----------------------------------------------------------------------
SensorReading megaReading;
SensorReading esp32Reading;

unsigned long lastMegaMessageTime = 0;
bool megaOnline = false;

unsigned long lastESP32SensorRead = 0;
unsigned long lastThingSpeakUpload = 0;
unsigned long lastWifiRetry = 0;

String megaLinkBuffer = "";

// Simple circular offline buffer
BufferedUpdate offlineBuffer[OFFLINE_BUFFER_MAX_ENTRIES];
int bufferHead = 0;
int bufferCount = 0;

bool lastUploadSuccess = false;

// -----------------------------------------------------------------------
// FUNCTION PROTOTYPES
// -----------------------------------------------------------------------
void connectWiFi();
void ensureWiFiConnected();
void initTime();
void readMegaLink();
void processMegaJson(const String &jsonStr);
void readLocalSensors();
void checkMegaOfflineTimeout();
int  buildStatusCode();
int  buildAlertCode();
bool uploadToThingSpeak(SensorReading mega, SensorReading esp, bool megaIsOnline);
void handleCloudFailure(SensorReading mega, SensorReading esp, bool megaIsOnline);
void flushOfflineBuffer();
void debugPrint(const String &message);
void printStatusBanner();

// =========================================================================
// SETUP
// =========================================================================
void setup() {
  Serial.begin(9600);
  MegaLink.begin(MEGA_LINK_BAUD_RATE, SERIAL_8N1, MEGA_LINK_RX_PIN, MEGA_LINK_TX_PIN);

#if ESP32_HAS_LOCAL_SENSORS
  dht.begin();
#endif

  debugPrint(F("================================"));
  debugPrint(String(NODE_DISPLAY_NAME) + " Gateway - BOOT");
  debugPrint(F("================================"));

  connectWiFi();
  ThingSpeak.begin(client);
  initTime();
}

// =========================================================================
// MAIN LOOP (non-blocking, millis()-based scheduling)
// =========================================================================
void loop() {
  ensureWiFiConnected();

  readMegaLink();               // non-blocking incremental read
  checkMegaOfflineTimeout();

  unsigned long now = millis();

  if (now - lastESP32SensorRead >= SENSOR_INTERVAL_LOCAL()) {
    lastESP32SensorRead = now;
    readLocalSensors();
  }

  if (now - lastThingSpeakUpload >= THINGSPEAK_INTERVAL_MS) {
    lastThingSpeakUpload = now;

    if (WiFi.status() == WL_CONNECTED) {
      bool ok = uploadToThingSpeak(megaReading, esp32Reading, megaOnline);
      lastUploadSuccess = ok;
      if (ok) {
        flushOfflineBuffer();
      } else {
        handleCloudFailure(megaReading, esp32Reading, megaOnline);
      }
    } else {
      handleCloudFailure(megaReading, esp32Reading, megaOnline);
    }

    printStatusBanner();
  }
}

// Helper so the "sensor interval" constant can live in config.h without
// clashing with the Mega's own constant of the same conceptual name.
unsigned long SENSOR_INTERVAL_LOCAL() {
  return 5000UL;
}

// =========================================================================
// connectWiFi() / ensureWiFiConnected()
// =========================================================================
void connectWiFi() {
  debugPrint("Connecting to WiFi: " + String(WIFI_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("WiFi connected");
    debugPrint("IP address: " + WiFi.localIP().toString());
  } else {
    debugPrint(F("WiFi connection FAILED (will retry in background)"));
  }
}

void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastWifiRetry >= WIFI_RETRY_INTERVAL_MS) {
      lastWifiRetry = now;
      debugPrint(F("WiFi disconnected. Attempting reconnect..."));
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

// =========================================================================
// initTime()  -- NTP sync for Africa/Lusaka (UTC+2, no DST)
// =========================================================================
void initTime() {
  if (WiFi.status() != WL_CONNECTED) return;
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  debugPrint(F("NTP time sync requested."));
}

// =========================================================================
// readMegaLink()
//   Accumulates characters from Serial2 until a newline is received,
//   then attempts to parse the line as JSON.
// =========================================================================
void readMegaLink() {
  while (MegaLink.available()) {
    char c = MegaLink.read();
    if (c == '\n') {
      processMegaJson(megaLinkBuffer);
      megaLinkBuffer = "";
    } else if (c != '\r') {
      megaLinkBuffer += c;
      // Prevent unbounded growth from noise/corruption.
      if (megaLinkBuffer.length() > 400) {
        megaLinkBuffer = "";
      }
    }
  }
}

// =========================================================================
// processMegaJson()
//   Parses and validates incoming JSON from the Mega. Rejects malformed
//   or incomplete payloads rather than trusting them blindly.
// =========================================================================
void processMegaJson(const String &jsonStr) {
  if (jsonStr.length() == 0) return;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);

  if (err) {
    debugPrint("Mega JSON parse error: " + String(err.c_str()));
    return;
  }

  if (!doc.containsKey("sensor_id")) {
    debugPrint(F("Mega JSON missing sensor_id, ignoring."));
    return;
  }

  String status = doc["status"] | "unknown";

  lastMegaMessageTime = millis();
  megaOnline = true;

  if (status == "ok" &&
      doc.containsKey("temperature") &&
      doc.containsKey("humidity") &&
      doc.containsKey("air_quality")) {
    megaReading.valid = true;
    megaReading.temperature = doc["temperature"];
    megaReading.humidity    = doc["humidity"];
    megaReading.airQuality  = doc["air_quality"];
  } else {
    // Mega reported a sensor error (e.g. DHT11 failure) -- do not use stale
    // values as if they were fresh; mark invalid for this cycle.
    megaReading.valid = false;
    debugPrint(F("Mega reported sensor error."));
  }
}

// =========================================================================
// checkMegaOfflineTimeout()
// =========================================================================
void checkMegaOfflineTimeout() {
  if (lastMegaMessageTime == 0) return; // never heard from Mega yet
  if (megaOnline && (millis() - lastMegaMessageTime > MEGA_OFFLINE_TIMEOUT_MS)) {
    megaOnline = false;
    megaReading.valid = false;
    debugPrint(F("Mega Station: OFFLINE"));
  }
}

// =========================================================================
// readLocalSensors()
//   Reads the ESP32's own DHT11/MQ-135 if physically present.
// =========================================================================
void readLocalSensors() {
#if ESP32_HAS_LOCAL_SENSORS
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int   aq = analogRead(ESP32_MQ135_PIN);

  if (isnan(h) || isnan(t)) {
    esp32Reading.valid = false;
    debugPrint(F("ESP32 local DHT11 SENSOR ERROR"));
    return;
  }

  esp32Reading.valid = true;
  esp32Reading.temperature = t;
  esp32Reading.humidity = h;
  esp32Reading.airQuality = aq;
#else
  esp32Reading.valid = false; // no local sensors installed
#endif
}

// =========================================================================
// buildStatusCode()
//   Encodes device/health status as a single ThingSpeak-friendly integer:
//     bit 0: ESP32 WiFi connected
//     bit 1: Mega online
//     bit 2: ESP32 local sensors valid (if installed)
//     bit 3: last ThingSpeak upload succeeded
// =========================================================================
int buildStatusCode() {
  int code = 0;
  if (WiFi.status() == WL_CONNECTED) code |= 0b0001;
  if (megaOnline)                    code |= 0b0010;
  if (esp32Reading.valid)            code |= 0b0100;
  if (lastUploadSuccess)             code |= 0b1000;
  return code;
}

// =========================================================================
// buildAlertCode()
//   0 = no alert, 1 = warning, 2 = critical
//   Mirrors the Mega's own thresholds for consistency on the dashboard.
// =========================================================================
int buildAlertCode() {
  int level = 0;

  if (megaReading.valid) {
    if (megaReading.temperature > TEMP_WARNING_THRESHOLD_LOCAL()) level = max(level, 2);
    if (megaReading.airQuality  > AIR_QUALITY_WARNING_THRESHOLD_LOCAL()) level = max(level, 2);
  }
  if (esp32Reading.valid) {
    if (esp32Reading.temperature > TEMP_WARNING_THRESHOLD_LOCAL()) level = max(level, 2);
    if (esp32Reading.airQuality  > AIR_QUALITY_WARNING_THRESHOLD_LOCAL()) level = max(level, 2);
  }
  if (!megaOnline) level = max(level, 1);

  return level;
}

// Local mirrors of the Mega's alert thresholds, kept here so the ESP32 can
// tag alert status independently even if the Mega link is degraded.
float TEMP_WARNING_THRESHOLD_LOCAL() { return 35.0; }
int   AIR_QUALITY_WARNING_THRESHOLD_LOCAL() { return 500; }

// =========================================================================
// uploadToThingSpeak()
//   Writes all 8 fields in a single update, respecting the platform's
//   minimum update interval (enforced by THINGSPEAK_INTERVAL_MS).
// =========================================================================
bool uploadToThingSpeak(SensorReading mega, SensorReading esp, bool megaIsOnline) {
  ThingSpeak.setField(1, mega.valid ? mega.temperature : 0);
  ThingSpeak.setField(2, mega.valid ? mega.humidity : 0);
  ThingSpeak.setField(3, mega.valid ? mega.airQuality : 0);
  ThingSpeak.setField(4, esp.valid ? esp.temperature : 0);
  ThingSpeak.setField(5, esp.valid ? esp.humidity : 0);
  ThingSpeak.setField(6, esp.valid ? esp.airQuality : 0);
  ThingSpeak.setField(7, buildStatusCode());
  ThingSpeak.setField(8, buildAlertCode());

  int httpCode = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);

  if (httpCode == 200) {
    debugPrint(F("Cloud Upload: SUCCESS"));
    return true;
  } else {
    debugPrint("Cloud Upload: FAILED (HTTP code " + String(httpCode) + ")");
    return false;
  }
}

// =========================================================================
// handleCloudFailure()
//   Queues the current reading into the offline circular buffer so it can
//   be uploaded once connectivity returns. Oldest entries are overwritten
//   once the buffer is full (bounded memory usage).
// =========================================================================
void handleCloudFailure(SensorReading mega, SensorReading esp, bool megaIsOnline) {
  BufferedUpdate &slot = offlineBuffer[bufferHead];
  slot.timestamp  = millis();
  slot.megaData   = mega;
  slot.esp32Data  = esp;
  slot.megaOnline = megaIsOnline;

  bufferHead = (bufferHead + 1) % OFFLINE_BUFFER_MAX_ENTRIES;
  if (bufferCount < OFFLINE_BUFFER_MAX_ENTRIES) bufferCount++;

  debugPrint("Reading buffered offline. Buffer size: " + String(bufferCount));
}

// =========================================================================
// flushOfflineBuffer()
//   Called after a successful upload; drains queued readings one at a
//   time on subsequent cycles to avoid violating ThingSpeak's rate limit.
// =========================================================================
void flushOfflineBuffer() {
  if (bufferCount == 0) return;

  // Compute the oldest entry's index in the circular buffer.
  int oldestIndex = (bufferHead - bufferCount + OFFLINE_BUFFER_MAX_ENTRIES) % OFFLINE_BUFFER_MAX_ENTRIES;
  BufferedUpdate &entry = offlineBuffer[oldestIndex];

  bool ok = uploadToThingSpeak(entry.megaData, entry.esp32Data, entry.megaOnline);
  if (ok) {
    bufferCount--;
    debugPrint("Flushed one buffered reading. Remaining: " + String(bufferCount));
  }
  // If it fails again, we simply try again on the next cycle -- the entry
  // stays in place because bufferCount was not decremented.
}

// =========================================================================
// printStatusBanner()
//   Serial diagnostic banner mirroring the format requested in the spec.
// =========================================================================
void printStatusBanner() {
  debugPrint(F("================================"));
  debugPrint(String(NODE_DISPLAY_NAME) + " Environment Gateway");
  debugPrint(F("================================"));
  debugPrint("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"));
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("IP: " + WiFi.localIP().toString());
    debugPrint("RSSI: " + String(WiFi.RSSI()) + " dBm");
  }
  debugPrint("ThingSpeak: " + String(lastUploadSuccess ? "CONNECTED" : "UNREACHABLE"));
  debugPrint("Mega: " + String(megaOnline ? "ONLINE" : "OFFLINE"));
  if (megaReading.valid) {
    debugPrint("Mega Temperature: " + String(megaReading.temperature, 1) + " C");
    debugPrint("Mega Humidity: " + String(megaReading.humidity, 1) + " %");
    debugPrint("Mega Air Quality: " + String(megaReading.airQuality));
  }
#if ESP32_HAS_LOCAL_SENSORS
  if (esp32Reading.valid) {
    debugPrint("ESP32 Temperature: " + String(esp32Reading.temperature, 1) + " C");
    debugPrint("ESP32 Humidity: " + String(esp32Reading.humidity, 1) + " %");
    debugPrint("ESP32 Air Quality: " + String(esp32Reading.airQuality));
  }
#endif
  debugPrint("Cloud Upload: " + String(lastUploadSuccess ? "SUCCESS" : "FAILED/BUFFERED"));
  debugPrint("Offline buffer: " + String(bufferCount) + "/" + String(OFFLINE_BUFFER_MAX_ENTRIES));
  debugPrint(F("================================"));
}

// =========================================================================
// debugPrint()
// =========================================================================
void debugPrint(const String &message) {
#if DEBUG_MODE
  Serial.println(message);
#endif
}
