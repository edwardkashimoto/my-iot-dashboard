/*
 * config.js
 * -----------------------------------------------------------------------
 * Public dashboard configuration.
 *
 * SECURITY NOTE:
 * Only the ThingSpeak READ API key belongs here. NEVER place the WRITE
 * API key in this file or anywhere in the dashboard/ folder — it is
 * served as plain static files and is visible to anyone who opens the
 * page. The WRITE key lives only inside the ESP32 firmware's config.h.
 *
 * If a WRITE key has ever been committed to a public repo, regenerate it
 * in the ThingSpeak channel settings before going to production.
 * -----------------------------------------------------------------------
 */

const CONFIG = {
  THINGSPEAK_CHANNEL_ID: "3445381",
  THINGSPEAK_READ_API_KEY: "8VNDEGPDRUZL4999", // safe to expose: read-only, channel-scoped

  // How often the dashboard polls ThingSpeak for new data (ms).
  // Kept in line with the ESP32's own upload interval to avoid hammering
  // the ThingSpeak API with redundant requests.
  REFRESH_INTERVAL_MS: 20000,

  // ThingSpeak field -> logical meaning, matching esp32_environment_gateway.ino
  FIELDS: {
    MEGA_TEMPERATURE: "field1",
    MEGA_HUMIDITY: "field2",
    MEGA_AIR_QUALITY: "field3",
    ESP32_TEMPERATURE: "field4",
    ESP32_HUMIDITY: "field5",
    ESP32_AIR_QUALITY: "field6",
    STATUS_CODE: "field7",
    ALERT_CODE: "field8",
  },

  NODES: {
    mega: {
      id: 1,
      key: "mega",
      name: "Mega Station 1",
      device: "Arduino Mega 2560",
      hasLightSensor: false,
    },
    esp32: {
      id: 2,
      key: "esp32",
      name: "ESP32 Station 1",
      device: "ESP32 DevKit V1",
      hasLightSensor: false, // set true only if a physical light sensor is wired
    },
  },

  // Thresholds mirrored from firmware — used purely for dashboard display
  // logic (status badges, alert coloring). Adjust to match config.h on
  // both boards if you change them there.
  THRESHOLDS: {
    TEMPERATURE: { warning: 30, critical: 35 },
    AIR_QUALITY: { good: 200, moderate: 400, warning: 500 }, // >500 = dangerous
  },

  // Mega link timeout mirrors MEGA_OFFLINE_TIMEOUT_MS in the ESP32 firmware.
  MEGA_OFFLINE_TIMEOUT_MS: 30000,
};
