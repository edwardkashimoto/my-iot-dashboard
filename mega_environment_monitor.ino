/*
=============================================================================
Distributed IoT Environmental Monitoring and Alert System
Arduino Mega 2560 - Sensor Node Firmware
=============================================================================

ROLE:
- Reads temperature and humidity from DHT11
- Reads air quality from MQ-135
- Sends JSON sensor data to ESP32 through Serial2
- Sends SMS alerts through SIM800L using Serial1
- Does NOT connect directly to Wi-Fi or ThingSpeak

HARDWARE:

DHT11
  DATA -> Mega Digital Pin 7

MQ-135
  AO -> Mega Analog Pin A0

SIM800L
  TX -> Mega RX1 = Pin 19
  RX -> Mega TX1 = Pin 18
  IMPORTANT: SIM800L requires appropriate voltage/power protection.

ESP32
  Mega TX2 = Pin 16 -> voltage divider -> ESP32 RX
  Mega RX2 = Pin 17 <- ESP32 TX

IMPORTANT:
Mega uses 5V logic.
ESP32 uses 3.3V logic.

The Mega TX2 signal MUST be reduced to approximately 3.3V
before connecting to the ESP32 RX pin.

=============================================================================
*/

#include <DHT.h>
#include "config.h"

// ============================================================================
// OBJECTS
// ============================================================================

DHT dht(DHTPIN, DHTTYPE);

// Serial2 is used for the Mega <-> ESP32 communication.
// Mega Serial2:
// TX2 = Pin 16
// RX2 = Pin 17
#define ESP32_LINK Serial2

// Serial1 is reserved for SIM800L.
// Mega Serial1:
// TX1 = Pin 18
// RX1 = Pin 19

// ============================================================================
// SENSOR STATE
// ============================================================================

float currentTemperature = NAN;
float currentHumidity = NAN;

int currentAirQuality = 0;

bool sensorValid = false;

// ============================================================================
// TIMERS
// ============================================================================

unsigned long lastSensorRead = 0;
unsigned long lastESP32Send = 0;

unsigned long lastTempAlertSent = 0;
unsigned long lastGasAlertSent = 0;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void readSensors();
void sendMegaDataToESP32();
void checkAlerts();

void initSIM800L();
void sendSMS(const String &message);

String buildJsonPayload();

void debugPrint(const String &message);

// ============================================================================
// SETUP
// ============================================================================

void setup()
{
    // USB Serial Monitor
    Serial.begin(SERIAL_BAUD_RATE);

    // ESP32 communication
    ESP32_LINK.begin(SERIAL_BAUD_RATE);

    // SIM800L communication
    Serial1.begin(SIM800_BAUD_RATE);

    // Start DHT11
    dht.begin();

    // Initialize SIM800L
    initSIM800L();

    debugPrint("================================");
    debugPrint(String(NODE_DISPLAY_NAME));
    debugPrint("Mega Environment Monitor - BOOT");
    debugPrint("================================");

    debugPrint("DHT11 Pin: D7");
    debugPrint("MQ-135 Pin: A0");
    debugPrint("ESP32 Link: Serial2");
    debugPrint("SIM800L Link: Serial1");
    debugPrint("================================");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop()
{
    unsigned long now = millis();

    // ---------------------------------------------------------
    // Read sensors
    // ---------------------------------------------------------

    if (now - lastSensorRead >= SENSOR_INTERVAL)
    {
        lastSensorRead = now;

        readSensors();
        checkAlerts();
    }

    // ---------------------------------------------------------
    // Send data to ESP32
    // ---------------------------------------------------------

    if (now - lastESP32Send >= ESP32_SEND_INTERVAL)
    {
        lastESP32Send = now;

        sendMegaDataToESP32();
    }
}

// ============================================================================
// READ SENSORS
// ============================================================================

void readSensors()
{
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    int airQuality = analogRead(MQ135_PIN);

    // Check DHT11
    if (isnan(humidity) || isnan(temperature))
    {
        sensorValid = false;

        debugPrint("DHT11 SENSOR ERROR");

        return;
    }

    // Store valid readings
    sensorValid = true;

    currentTemperature = temperature;
    currentHumidity = humidity;
    currentAirQuality = airQuality;

    // Display readings
    debugPrint("--------------------------------");
    debugPrint(String(NODE_DISPLAY_NAME));
    debugPrint(
        "Temperature: " +
        String(currentTemperature, 1) +
        " C"
    );

    debugPrint(
        "Humidity: " +
        String(currentHumidity, 1) +
        " %"
    );

    debugPrint(
        "Air Quality: " +
        String(currentAirQuality)
    );

    debugPrint("--------------------------------");
}

// ============================================================================
// BUILD JSON PAYLOAD
// ============================================================================

String buildJsonPayload()
{
    String json = "{";

    json += "\"node\":";
    json += String(NODE_ID);
    json += ",";

    json += "\"sensor_id\":\"";
    json += String(NODE_NAME);
    json += "\",";

    if (sensorValid)
    {
        json += "\"temperature\":";
        json += String(currentTemperature, 1);
        json += ",";

        json += "\"humidity\":";
        json += String(currentHumidity, 1);
        json += ",";

        json += "\"air_quality\":";
        json += String(currentAirQuality);
        json += ",";

        json += "\"status\":\"ok\"";
    }
    else
    {
        json += "\"status\":\"sensor_error\"";
    }

    json += "}";

    return json;
}

// ============================================================================
// SEND DATA TO ESP32
// ============================================================================

void sendMegaDataToESP32()
{
    String payload = buildJsonPayload();

    // Send clean JSON through Serial2
    ESP32_LINK.println(payload);

    debugPrint(
        "Sending data to ESP32: " +
        payload
    );
}

// ============================================================================
// CHECK ALERT CONDITIONS
// ============================================================================

void checkAlerts()
{
    // Don't generate alerts if the DHT11 reading failed.
    if (!sensorValid)
    {
        return;
    }

    unsigned long now = millis();

    // ---------------------------------------------------------
    // TEMPERATURE ALERT
    // ---------------------------------------------------------

    if (currentTemperature > TEMP_WARNING_THRESHOLD)
    {
        if (
            lastTempAlertSent == 0 ||
            now - lastTempAlertSent >= SMS_COOLDOWN_MS
        )
        {
            String message = "";

            message += "ENVIRONMENT ALERT\n\n";
            message += "Node: ";
            message += String(NODE_DISPLAY_NAME);
            message += "\n";

            message += "Condition: High Temperature\n";

            message += "Temperature: ";
            message += String(currentTemperature, 1);
            message += " C\n";

            message += "Threshold: ";
            message += String(TEMP_WARNING_THRESHOLD, 1);
            message += " C\n\n";

            message += "Please investigate the environment.";

            sendSMS(message);

            lastTempAlertSent = now;
        }
    }

    // ---------------------------------------------------------
    // AIR QUALITY ALERT
    // ---------------------------------------------------------

    if (currentAirQuality > AIR_QUALITY_WARNING_THRESHOLD)
    {
        if (
            lastGasAlertSent == 0 ||
            now - lastGasAlertSent >= SMS_COOLDOWN_MS
        )
        {
            String message = "";

            message += "ENVIRONMENT ALERT\n\n";

            message += "Node: ";
            message += String(NODE_DISPLAY_NAME);
            message += "\n";

            message += "Condition: Poor Air Quality\n";

            message += "Air Quality: ";
            message += String(currentAirQuality);
            message += "\n";

            message += "Threshold: ";
            message += String(AIR_QUALITY_WARNING_THRESHOLD);
            message += "\n\n";

            message += "Please investigate the environment.";

            sendSMS(message);

            lastGasAlertSent = now;
        }
    }
}

// ============================================================================
// INITIALIZE SIM800L
// ============================================================================

void initSIM800L()
{
    debugPrint("Initializing SIM800L...");

    Serial1.println("AT");
    delay(500);

    Serial1.println("AT+CMGF=1");
    delay(500);

    debugPrint("SIM800L initialization complete.");
}

// ============================================================================
// SEND SMS
// ============================================================================

void sendSMS(const String &message)
{
    debugPrint("Sending SMS alert...");

    // Set SMS text mode
    Serial1.println("AT+CMGF=1");
    delay(500);

    // Set destination number
    Serial1.print("AT+CMGS=\"");
    Serial1.print(SMS_DESTINATION_NUMBER);
    Serial1.println("\"");

    delay(1000);

    // SMS message body
    Serial1.print(message);

    delay(500);

    // CTRL+Z tells SIM800L to send the SMS
    Serial1.write(26);

    delay(5000);

    debugPrint("SMS alert dispatched.");
}

// ============================================================================
// DEBUG PRINT
// ============================================================================

void debugPrint(const String &message)
{
#if DEBUG_MODE
    Serial.println(message);
#endif
}