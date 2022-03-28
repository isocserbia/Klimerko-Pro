#include <SoftwareSerial.h>
#include <FastLED.h>
#include <PMserial.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <NTPClient.h>        // https://github.com/taranais/NTPClient/blob/master/NTPClient.h
#include <WiFiUDP.h>
#include <uptime_formatter.h> // https://github.com/YiannisBourkelis/Uptime-Library
#include "rom/rtc.h"          // https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/ResetReason/ResetReason.ino
#include <movingAvg.h>
#include <esp_task_wdt.h>

// -------------------------- Serial Print Macros ---------------------------------------
#define spln(a)      (Serial.println(a))
#define sp(a)        (Serial.print(a))
#define spf(a, b)    (Serial.printf(a, b))

// -------------------------- Pin Definitions -------------------------------------------
#define NO2_RX_PIN      33
#define NO2_TX_PIN      32
#define SO2_RX_PIN      26
#define SO2_TX_PIN      25
#define PMS_RX_PIN      27
#define PMS_TX_PIN      13
#define WIFI_CONFIG_PIN 18
#define RGB_PIN         19
#define RGB_NUM_LEDS     1

// -------------------------- WiFi ------------------------------------------------------
const int      wifiReconnectInterval    = 10;
bool           wifiConnectionLost       = false;
unsigned long  wifiReconnectLastAttempt;

// -------------------------- WiFi Configuration Portal ---------------------------------
#define        WM_DEBUG_LEVEL                       DEBUG_NOTIFY // Debug level for WiFi Configuration Portal
char const     *wifiConfigPortalPassword            = "ConfigMode"; // Password for WiFi Configuration Portal WiFi Network
const int      wifiConfigTimeout                    = 1800; // Seconds before WiFi Configuration expires after no activity
char           wifiConfigPortalSSID[128];
bool           wifiConfigActive                     = false; // If WiFi Configuration Portal is currently active
unsigned long  wifiConfigActiveSince                = 0;
const int      wifiConfigButtonLongPressTime        = 1000; // (milliseconds) What's considered a long wifi config button press
const int      wifiConfigButtonShortPressTime       = 100; // (milliseconds) Everything above this and below LongPressTime is considered a short press
unsigned long  wifiConfigButtonPressedTime          = 0;
unsigned long  wifiConfigButtonReleasedTime         = 0;
bool           wifiConfigButtonPressed              = false;
bool           wifiConfigButtonLongPressDetected    = false;
int            wifiConfigButtonCurrentState;
int            wifiConfigButtonLastState            = HIGH;

// -------------------------- MQTT ------------------------------------------------------
String         klimerkoID;                  // Based on ESP32 Chip ID

char           MQTT_SERVER[32]              = "api.decazavazduh.rs";
const uint16_t MQTT_PORT                    = 1883;
char           MQTT_CLIENT_ID[64];
char*          MQTT_USERNAME;
char           MQTT_PASSWORD[64];
uint16_t       MQTT_MAX_MESSAGE_SIZE        = 2048;

const int      mqttReconnectInterval        = 15;    // Seconds between retries
bool           mqttConnectionLost           = false;
unsigned long  mqttReconnectLastAttempt;

const int      metadataPublishInterval      = 900;   // [seconds] How often to send metadata to platform
const int      metadataPublishBootInterval  = 70;    // [seconds] How long after boot to send initial package of metadata
bool           metadataPublishBootDone      = false; // If the initial metadata at boot is sent or not
unsigned long  metadataLastPublish;

// -------------------------- Firmware Update (GitHub) -------------------------------------
const String   firmwareVersion                  = "0.9.8";
const char*    firmwareVersionPortal            =  "<p>Firmware Version: 0.9.8</p>";
int            firmwareUpdateCheckInterval      = 5400; // [5400 = 1.5h] Seconds between firmware update checks
unsigned long  firmwareUpdateLastCheck          = firmwareUpdateCheckInterval * 1000; // So that it checks right at boot time
const String   firmwareUpdateFirmwareURL        = "https://raw.githubusercontent.com/isocserbia/Klimerko-Pro/main/firmware/firmware.bin";
const String   firmwareUpdateFirmwareVersionURL = "https://raw.githubusercontent.com/isocserbia/Klimerko-Pro/main/firmware/firmware-version";

// For preferences library, to keep track of failed/successful updates
String         lastSuccessfulOTA;
const char*    preferences_lastSuccessfulOTA          = "lastSuccOTA";
const char*    preferences_lastSuccessfulOTADefault   = "NO INFO";
String         lastFailedOTA;
const char*    preferences_lastFailedOTA              = "lastFailOTA";
const char*    preferences_lastFailedOTADefault       = "NO INFO";

// This is the GitHub DigiCert High Assurance EV Root CA that expires on Mon, 10 Nov 2031 00:00:00 GMT
const char*    fwRootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
"-----END CERTIFICATE-----\n";

// -------------------------- SENSORS GENERAL -------------------------------------------
// TODO, publish metadata about 10 seconds after first sensor read
int            sensorDataPublishInterval            = 60;   // [seconds] [DEFAULT] How often to send sensor data. This variable changes depending on whats in device's memory
const int      sensorDataPublishIntervalMax         = 600;  // Maximum user-settable data publishing interval
const int      sensorDataPublishIntervalMin         = 30;   // Minimum user-settable data publishing interval
int            sensorAveragingSamples               = 10;   // How many averaging samples to collect and average from
int            sensorDataReadInterval               = sensorDataPublishInterval/sensorAveragingSamples; // How often to read sensor data (and average)
const int      sensorDataReadIntervalWhenConsideredOffline = sensorDataPublishInterval * 2; // How often to read sensor (therefore check if it's available again) if it's considered that it isn't connected to the board
const uint8_t  sensorRetriesBeforeConsideredOffline = 5;    // After how many read attempts should the sensor be considered (and published as) offline and thus fall back to less frequent readings
const int      sensorSerialWaitTime                 = 1500; // Milliseconds to wait before considering the sensor is unresponsive to the sent command
unsigned long  sensorDataLastPublish;
unsigned long  sensorDataLastRead;
unsigned long  publishSensorDataLoopCurrentTime; // Used to keep track of time data started to be read & published instead of when it finished, so the intervals seen from the platform are more precise

const char*    preferences_sensorDataPublishInterval = "pubInterval";
int            preferences_sensorDataPublishIntervalDefault = sensorDataPublishInterval;
const char*    preferences_LastZeroingDefault       = "NO INFO";
const char*    preferences_LastFailedZeroingDefault = "NO INFO";
const char*    preferences_SerialNumberDefault      = "NO INFO";


// -------------------------- SO2 Sensor ------------------------------------------------
#define        SO2_BAUD 9600             // Sensor Baud Rate
bool           so2SensorOnline           = true; // Needs to be true so the metadata isn't sent at boot when the sensor initializes AND it can be flagged false once it fails during operation
bool           so2SensorReady            = false;
uint8_t        so2SensorRetryNumber      = 0;    // Current number of times the sensor has failed to respond
unsigned long  so2SensorLastRecoveryAttemptTime; // Last time the sensor was checked while in less frequent reading mode (due to being offline)

const float    so2MolarMass = 64.0638;     // SO2 Molar Mass
int            so2AverageConcentration;    // Averaged ug/m3 value
int            so2CurrentConcentration;    // Current ug/m3 Value
int            so2CurrentConcentrationPPB; // Current value straight from sensor (PPB)
int            so2CurrentConcentrationADC; // Current value from ADC converter
int            so2AverageTemperature;      // Averaged value
int            so2CurrentTemperature;
int            so2CurrentTemperatureDigital;
int            so2AverageHumidity;         // Averaged value
int            so2CurrentHumidity;
int            so2CurrentHumidityDigital;
String         so2SensorUptime;
int            so2SensorUptime_Days, so2SensorUptime_Hours, so2SensorUptime_Minutes, so2SensorUptime_Seconds;

String         so2Firmware;
String         so2SerialNumber;
const char*    preferences_so2SerialNumber      = "so2Serial";
String         so2LastZeroing;
const char*    preferences_so2LastZeroing       = "so2Zeroed";
String         so2LastFailedZeroing;
const char*    preferences_so2LastFailedZeroing = "so2ZeroFailed";

// -------------------------- NO2 Sensor ------------------------------------------------
#define        NO2_BAUD 9600             // Sensor Baud Rate
bool           no2SensorOnline           = true;
bool           no2SensorReady            = false;
uint8_t        no2SensorRetryNumber      = 0;    // Current number of times the sensor has failed to respond
unsigned long  no2SensorLastRecoveryAttemptTime; // Last time the sensor was checked while in less frequent reading mode (due to being offline)

const float    no2MolarMass = 46.0055;           // NO2 Molar Mass
int            no2AverageConcentration;          // Averaged ug/m3 value
int            no2CurrentConcentration;          // Current ug/m3 Value
int            no2CurrentConcentrationPPB;       // Current value straight from sensor (PPB)
int            no2CurrentConcentrationADC;       // Current value from ADC converter
int            no2AverageTemperature;            // Averaged value
int            no2CurrentTemperature;
int            no2CurrentTemperatureDigital;
int            no2AverageHumidity;               // Averaged value
int            no2CurrentHumidity;
int            no2CurrentHumidityDigital;
String         no2SensorUptime;
int            no2SensorUptime_Days, no2SensorUptime_Hours, no2SensorUptime_Minutes, no2SensorUptime_Seconds;

String         no2Firmware;
String         no2SerialNumber;
const char*    preferences_no2SerialNumber      = "no2Serial"; // Variable name the way it will be stored in persistant memory
String         no2LastZeroing;
const char*    preferences_no2LastZeroing       = "no2Zeroed"; // Variable name the way it will be stored in persistant memory
String         no2LastFailedZeroing;
const char*    preferences_no2LastFailedZeroing = "no2ZeroFailed"; // Variable name the way it will be stored in persistant memory

// -------------------------- PMS Sensor ------------------------------------------------
#define        PMS_BAUD 9600                     // Sensor Baud Rate
bool           pmsSensorOnline                  = true;
uint8_t        pmsSensorRetryNumber             = 0;
unsigned long  pmsSensorLastRecoveryAttemptTime; // Last time the sensor was checked while in less frequent reading mode (due to being offline)

int            pm1Current;
int            pm1Average;                       // Averaged value
int            pm2_5Current;
int            pm2_5Average;                     // Averaged value
int            pm10Current;
int            pm10Average;                      // Averaged value
unsigned long  pmsLastRead;

// -------------------------- RGB LED ---------------------------------------------------
bool           rgbEffect_WiFi                  = false;
bool           rgbEffect_WiFiExpire            = false;
bool           rgbEffect_WiFi_FadingUp         = false;
uint8_t        rgbEffect_WiFi_FadeLevel        = 0;
const int      rgbEffect_WiFi_Interval         = 1500;
unsigned long  rgbEffect_WiFi_PrevTime;

bool           rgbEffect_Mqtt                  = false;
bool           rgbEffect_MqttExpire            = false;
bool           rgbEffect_Mqtt_FadingUp         = false;
uint8_t        rgbEffect_Mqtt_FadeLevel        = 0;
uint8_t        rgbEffect_Mqtt_FadeLevelGreen   = 0;
const int      rgbEffect_Mqtt_Interval         = 800;
unsigned long  rgbEffect_Mqtt_PrevTime;

bool           rgbEffect_WiFiConfig            = false;
bool           rgbEffect_WiFiConfigExpire      = false;
bool           rgbEffect_WiFiConfig_FadingUp   = false;
uint8_t        rgbEffect_WiFiConfig_FadeLevel  = 0;
const int      rgbEffect_WiFiConfig_Interval   = 500;
unsigned long  rgbEffect_WiFiConfig_PrevTime;

bool           rgbEffect_GreenBlink            = false;
bool           rgbEffect_GreenBlink_FadingUp   = false;
int            rgbEffect_GreenBlink_Count      = 0;
const int      rgbEffect_GreenBlink_MaxBlinks  = 10;
const int      rgbEffect_GreenBlink_Interval   = 100;
unsigned long  rgbEffect_GreenBlink_PrevTime;

bool           rgbOtaSwitch                    = false;
const int      rgbOtaInterval                  = 200;
unsigned long  rgbOtaLastChange;

// -------------------------- Other -----------------------------------------------------
String         resetReason    = "UNKNOWN";
const int      wdtTimeout     = 90; // If the device hangs for this many seconds, reset it

// -------------------------- Objects ---------------------------------------------------
SoftwareSerial no2Serial;
SoftwareSerial so2Serial;
CRGB rgb[RGB_NUM_LEDS];
SerialPM pms(PMS7003, PMS_RX_PIN, PMS_TX_PIN); // https://github.com/avaldebe/PMserial/tree/master/examples/SoftwareSerial
WiFiManager wm;
WiFiManagerParameter portalMqttPassword("mqtt_password", "Platform Password", "do not change unless instructed", 64);
WiFiManagerParameter portalDisplayFirmwareVersion(firmwareVersionPortal);
WiFiManagerParameter portalDisplayCredits("Hardware & Firmware Designed, Developed and Maintained by Vanja Stanic");
WiFiClient networkClient;
PubSubClient mqtt(networkClient);
Preferences preferences;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
movingAvg avgSo2(sensorAveragingSamples);
movingAvg avgSo2Temp(sensorAveragingSamples);
movingAvg avgSo2Hum(sensorAveragingSamples);
movingAvg avgNo2(sensorAveragingSamples);
movingAvg avgNo2Temp(sensorAveragingSamples);
movingAvg avgNo2Hum(sensorAveragingSamples);
movingAvg avgPM1(sensorAveragingSamples);
movingAvg avgPM25(sensorAveragingSamples);
movingAvg avgPM10(sensorAveragingSamples);

// Forward-declaration
bool initSO2();
bool initNO2();
bool readSO2();
bool readNO2();
void publishMetadata();

void readPersistantStorage() {
  String TEMP_MQTT_PASSWORD;
  preferences.begin("klimerko", false);
  TEMP_MQTT_PASSWORD   = preferences.getString("mqtt_password", "UNDEFINED"); // Load the "mqtt_password" stored in memory and make it "UNDEFINED" if it doesn't already exist
  so2SerialNumber      = preferences.getString(preferences_so2SerialNumber, preferences_SerialNumberDefault);
  so2LastZeroing       = preferences.getString(preferences_so2LastZeroing, preferences_LastZeroingDefault);
  so2LastFailedZeroing = preferences.getString(preferences_so2LastFailedZeroing, preferences_LastFailedZeroingDefault);
  no2SerialNumber      = preferences.getString(preferences_no2SerialNumber, preferences_SerialNumberDefault);
  no2LastZeroing       = preferences.getString(preferences_no2LastZeroing, preferences_LastZeroingDefault);
  no2LastFailedZeroing = preferences.getString(preferences_no2LastFailedZeroing, preferences_LastFailedZeroingDefault);
  lastSuccessfulOTA    = preferences.getString(preferences_lastSuccessfulOTA, preferences_lastSuccessfulOTADefault);
  lastFailedOTA        = preferences.getString(preferences_lastFailedOTA, preferences_lastFailedOTADefault);
  sensorDataPublishInterval = preferences.getInt(preferences_sensorDataPublishInterval, preferences_sensorDataPublishIntervalDefault);
  preferences.end();
  TEMP_MQTT_PASSWORD.toCharArray(MQTT_PASSWORD, 64);

  sp("[Persistant Storage] MQTT Password: ");
  spln(MQTT_PASSWORD);
  sp("[Persistant Storage] SO2 Serial Number at: ");
  spln(so2SerialNumber);
  sp("[Persistant Storage] SO2 Last Zeroed at: ");
  spln(so2LastZeroing);
  sp("[Persistant Storage] SO2 Last Failed Zeroing at: ");
  spln(so2LastFailedZeroing);
  sp("[Persistant Storage] NO2 Serial Number at: ");
  spln(no2SerialNumber);
  sp("[Persistant Storage] NO2 Last Zeroed at: ");
  spln(no2LastZeroing);
  sp("[Persistant Storage] NO2 Last Failed Zeroing at: ");
  spln(no2LastFailedZeroing);
  sp("[Persistant Storage] Last Succesful OTA Update at: ");
  spln(lastSuccessfulOTA);
  sp("[Persistant Storage] Last Failed OTA Update at: ");
  spln(lastFailedOTA);
  sp("[Persistant Storage] Sensor Data Publishing Interval: ");
  spln(sensorDataPublishInterval);
}

bool initSO2() {
  sp("[SO2] Initializing... ");
  so2Serial.begin(SO2_BAUD, SWSERIAL_8E1, SO2_RX_PIN, SO2_TX_PIN, false);
  so2Serial.flush();
  delay(200); // TODO: Sensor fails to init (once) at first boot (when the board is first plugged into power)
  so2Serial.write("fw");
  unsigned long sensorSerialWaitStart = millis();
  while (!so2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("Failed! Sensor didn't reply in time.");
      so2SensorOnline = false;
      return false;
    }
  }
  String checkString = so2Serial.readStringUntil('\r');
  sp("Sensor Returned: ");
  sp(checkString);
  sp(", which is ");
  sp(checkString.length());
  sp(" characters long. ");
  if (checkString.length() == 7) {
    so2SensorRetryNumber = 0;
    so2Firmware = checkString;
    delay(200);
    so2Serial.write("\r"); // Seems to be required so the sensor doesn't return an empty string on next read
    delay(500);
    so2Serial.flush();
    delay(100);
    spln("Successfully initialized!");
    if (!so2SensorOnline) { // If the sensor was previously offline during operation, send metadata to update that it's online
      so2SensorOnline = true;
      readSO2(); // Needs to read SO2 sensor data so that the sensor part of the metadata payload isn't empty
      publishMetadata();
    }
    return true;
  }
  spln("Failed! Sensor didn't return the correct Firmware Version length.");
  so2SensorOnline = false;
  return false;
}

bool initNO2() {
  sp("[NO2] Initializing... ");
  no2Serial.begin(NO2_BAUD, SWSERIAL_8E1, NO2_RX_PIN, NO2_TX_PIN, false);
  no2Serial.flush();
  delay(200);
  no2Serial.write("fw");
  unsigned long sensorSerialWaitStart = millis();
  while (!no2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("Failed! Sensor didn't reply in time.");
      no2SensorOnline = false;
      return false;
    }
  }
  String checkString = no2Serial.readStringUntil('\r');
  sp("Sensor Returned: ");
  sp(checkString);
  sp(", which is ");
  sp(checkString.length());
  sp(" characters long. ");
  if (checkString.length() == 7) {
    no2SensorRetryNumber = 0;
    no2Firmware = checkString;
    delay(200);
    no2Serial.write("\r"); // Seems to be required so the sensor doesn't return an empty string on next read
    delay(500);
    no2Serial.flush();
    delay(100);
    spln("Successfully initialized!");
    if (!no2SensorOnline) { // If the sensor was previously offline during operation, send metadata to update that it's online
      no2SensorOnline = true;
      readNO2(); // Needs to read NO2 sensor data so that the sensor part of metadata isn't empty
      publishMetadata();
    }
    return true;
  }
  spln("Failed! Sensor didn't return the correct Firmware Version length.");
  no2SensorOnline = false;
  return false;
}

void publishMetadata() {
  sp("[DATA] Sending metadata to platform: ");
  timeClient.update();
  char JSONmessageBuffer[2048];

  DynamicJsonDocument doc(2048);
  doc["type"] = "device_metadata";
  doc["client_id"] = MQTT_CLIENT_ID;
  doc["correlation_id"] = MQTT_CLIENT_ID;
  doc["sent_at"] = timeClient.getFormattedDate();

  JsonObject data = doc.createNestedObject("data");

  // Klimerko itself
  data["sent_at"]                        = timeClient.getFormattedDate();
  data["device_fw"]                      = firmwareVersion;
  data["device_active_time"]             = uptime_formatter::getUptime(); // esp_timer_get_time
  data["device_wifi_rssi"]               = WiFi.RSSI();
  data["device_free_heap"]               = ESP.getFreeHeap();
  data["device_flash_size"]              = ESP.getFlashChipSize();
  data["device_sketch_used"]             = ESP.getSketchSize();
  data["device_sketch_total"]            = (ESP.getSketchSize() + ESP.getFreeSketchSpace());
  data["device_last_successful_ota"]     = lastSuccessfulOTA;
  data["device_last_failed_ota"]         = lastFailedOTA;
  data["device_last_reset_reason"]       = resetReason;
  data["device_sensor_read_interval"]    = sensorDataReadInterval;
  data["device_sensor_publish_interval"] = sensorDataPublishInterval;

  // SO2
  data["so2_online"]              = so2SensorOnline;
  data["so2_ready"]               = so2SensorReady;
  data["so2_active_time"]         = so2SensorUptime;
  data["so2_serial"]              = so2SerialNumber;
  data["so2_fw_version"]          = so2Firmware;
  data["so2_last_zeroing"]        = so2LastZeroing;
  data["so2_last_failed_zeroing"] = so2LastFailedZeroing;

  // NO2
  data["no2_online"]              = no2SensorOnline;
  data["no2_ready"]               = no2SensorReady;
  data["no2_active_time"]         = no2SensorUptime;
  data["no2_serial"]              = no2SerialNumber;
  data["no2_fw_version"]          = so2Firmware;
  data["no2_last_zeroing"]        = no2LastZeroing;
  data["no2_last_failed_zeroing"] = no2LastFailedZeroing;

  // PMS
  data["pms_online"]              = pmsSensorOnline;

  serializeJson(doc, JSONmessageBuffer);
  serializeJson(doc, Serial);
  spln("");

  if (mqtt.publish("v1/devices/actions", JSONmessageBuffer, true)) {
    spln("[MQTT] Metadata sent!");
  } else {
    spln("[MQTT] Metadata failed to send.");
  }
}

void publishMetadataLoop() {
  if (millis() - metadataLastPublish >= metadataPublishInterval * 1000) {
    publishMetadata();
    metadataLastPublish = millis();
  }

  if (!metadataPublishBootDone) {
    if (millis() >= metadataPublishBootInterval * 1000) {
      publishMetadata();
      metadataPublishBootDone = true;
    }
  }
}

int ppb_to_ugm3(int value, int temperature, float molarMass) {
  return (((float)value)*(12.187)*(molarMass))/(273.15+(float)temperature);
}

bool readSO2() {
  if (so2SensorRetryNumber >= sensorRetriesBeforeConsideredOffline || !so2SensorOnline) {
    spln("[SO2] Sensor seems to be offline. Trying to re-initialize it...");
    so2Serial.end();
    if (so2SensorOnline) {
      so2SensorOnline = false;
      // Reset the averaging since we don't know how long the sensor was offline
      spln("[SO2] Averaging Values Reset Because the Sensor is Offline");
      avgSo2.reset();
      avgSo2Temp.reset();
      avgSo2Hum.reset();
      publishMetadata();
    }
    if (!initSO2()) {
      return false;
    }
  }
  String dataString, currentSerialNumber;
  long dataArray[11];
  so2Serial.write("\r"); // This is better than "c" because c causes continous output.
  unsigned long sensorSerialWaitStart = millis();
  while (!so2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      so2SensorRetryNumber ++;
      sp("[SO2] Couldn't get data this time. ");
      sp(so2SensorRetryNumber);
      sp("/");
      spln(sensorRetriesBeforeConsideredOffline);
      return false;
    }
  }
  dataString = so2Serial.readStringUntil('\n');
  currentSerialNumber = dataString.substring(0, dataString.indexOf(','));
  sp("[SO2 RAW]: ");
  spln(dataString);
  for (int i = 0; i < 11; i++) {
    String subS = dataString.substring(0, dataString.indexOf(','));
    if (subS.length() == 0) return false;
    dataArray[i] = subS.toInt();
    dataString = dataString.substring(dataString.indexOf(',') + 2);
  }

  so2SensorOnline = true;
  so2SensorRetryNumber = 0;

  so2CurrentConcentrationPPB    = dataArray[1];
  so2CurrentTemperature         = dataArray[2] - 2; // Temperature offset 
  so2CurrentHumidity            = dataArray[3] - 1; // Humidity offset
  so2CurrentConcentrationADC    = dataArray[4];
  so2CurrentTemperatureDigital  = dataArray[5];
  so2CurrentHumidityDigital     = dataArray[6];
  so2SensorUptime_Days          = dataArray[7];
  so2SensorUptime_Hours         = dataArray[8];
  so2SensorUptime_Minutes       = dataArray[9];
  so2SensorUptime_Seconds       = dataArray[10];

  so2CurrentConcentration = ppb_to_ugm3(so2CurrentConcentrationPPB, so2CurrentTemperature, so2MolarMass);

  so2AverageConcentration = avgSo2.reading(so2CurrentConcentration);
  so2AverageTemperature   = avgSo2Temp.reading(so2CurrentTemperature);
  so2AverageHumidity      = avgSo2Hum.reading(so2CurrentHumidity);

  so2SensorUptime = String(so2SensorUptime_Days) + " days, " + String(so2SensorUptime_Hours) + " hours, " + String(so2SensorUptime_Minutes) + " minutes, " + String(so2SensorUptime_Seconds) + " seconds";
  if (so2SensorUptime_Hours >= 3 || so2SensorUptime_Days > 0) {
    so2SensorReady = true;
  } else {
    so2SensorReady = false;
  }

  if (currentSerialNumber.length() == 12) {
    if (currentSerialNumber != so2SerialNumber) {
      sp("[SO2] Sensor Seems to be changed! The Serial Number in memory is '");
      sp(so2SerialNumber);
      sp("' while the new one is '");
      sp(currentSerialNumber);
      spln("'. Writing change to memory and resetting Zeroing Information.");
      so2SerialNumber = currentSerialNumber;
      preferences.begin("klimerko", false);
      preferences.putString(preferences_so2SerialNumber, so2SerialNumber);
      preferences.putString(preferences_so2LastZeroing, preferences_LastZeroingDefault);
      preferences.putString(preferences_so2LastFailedZeroing, preferences_LastFailedZeroingDefault);
      preferences.end();
      publishMetadata();
      // Reset the averages since it's a new sensor
      avgSo2.reset();
      avgSo2Temp.reset();
      avgSo2Hum.reset();
    }
  }

  sp("[SO2 PARSED] S/N: ");
  sp(so2SerialNumber);
  sp(", Conc (ug/m3): ");
  sp(so2CurrentConcentration);
  sp(", Conc Avg (ug/m3): ");
  sp(so2AverageConcentration);
  sp(", Conc (PPB): ");
  sp(so2CurrentConcentrationPPB);
  sp(", ADC: ");
  sp(so2CurrentConcentrationADC);
  sp(", Temp: ");
  sp(so2CurrentTemperature);
  sp(", Temp Avg: ");
  sp(so2AverageTemperature);
  sp(", Hum: ");
  sp(so2CurrentHumidity);
  sp(", Hum Avg: ");
  sp(so2AverageHumidity);
  sp(", TempDigital: ");
  sp(so2CurrentTemperatureDigital);
  sp(", HumDigital: ");
  sp(so2CurrentHumidityDigital);
  sp(", Online Since: ");
  sp(so2SensorUptime_Days);
  sp(":");
  sp(so2SensorUptime_Hours);
  sp(":");
  sp(so2SensorUptime_Minutes);
  sp(":");
  spln(so2SensorUptime_Seconds);

  return true;
}

bool readNO2() {
  if (no2SensorRetryNumber >= sensorRetriesBeforeConsideredOffline || !no2SensorOnline) {
    spln("[NO2] Sensor seems to be offline. Trying to re-initialize it...");
    no2Serial.end();
    if (no2SensorOnline) {
      no2SensorOnline = false;
      // Reset the averaging since we don't know how long the sensor was offline
      spln("[NO2] Averaging Values Reset Because the Sensor is Offline");
      avgNo2.reset();
      avgNo2Temp.reset();
      avgNo2Hum.reset();
      publishMetadata();
    }
    if (!initNO2()) {
      return false;
    }
  }
  String dataString, currentSerialNumber;
  long dataArray[11];
  no2Serial.write("\r"); // This is better than "c" because c causes continous output.
  unsigned long sensorSerialWaitStart = millis();
  while (!no2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      no2SensorRetryNumber ++;
      sp("[NO2] Couldn't get data this time. ");
      sp(no2SensorRetryNumber);
      sp("/");
      spln(sensorRetriesBeforeConsideredOffline);
      return false;
    }
  }
  dataString = no2Serial.readStringUntil('\n');
  currentSerialNumber = dataString.substring(0, dataString.indexOf(','));
  sp("[NO2 RAW]: ");
  spln(dataString);
  for (int i = 0; i < 11; i++) {
    String subS = dataString.substring(0, dataString.indexOf(','));
    if (subS.length() == 0) return false;
    dataArray[i] = subS.toInt();
    dataString = dataString.substring(dataString.indexOf(',') + 2);
  }

  no2SensorOnline = true;
  no2SensorRetryNumber = 0;

  no2CurrentConcentrationPPB   = dataArray[1];
  no2CurrentTemperature        = dataArray[2] - 2; // Offset
  no2CurrentHumidity           = dataArray[3] - 1; // Offset
  no2CurrentConcentrationADC   = dataArray[4];
  no2CurrentTemperatureDigital = dataArray[5];
  no2CurrentHumidityDigital    = dataArray[6];
  no2SensorUptime_Days         = dataArray[7];
  no2SensorUptime_Hours        = dataArray[8];
  no2SensorUptime_Minutes      = dataArray[9];
  no2SensorUptime_Seconds      = dataArray[10];

  no2CurrentConcentration = ppb_to_ugm3(no2CurrentConcentrationPPB, no2CurrentTemperature, no2MolarMass);

  no2AverageConcentration = avgNo2.reading(no2CurrentConcentration);
  no2AverageTemperature   = avgNo2Temp.reading(no2CurrentTemperature);
  no2AverageHumidity      = avgNo2Hum.reading(no2CurrentHumidity);

  no2SensorUptime = String(no2SensorUptime_Days) + " days, " + String(no2SensorUptime_Hours) + " hours, " + String(no2SensorUptime_Minutes) + " minutes, " + String(no2SensorUptime_Seconds) + " seconds";
  if (no2SensorUptime_Hours >= 3 || no2SensorUptime_Days > 0) {
    no2SensorReady = true;
  } else {
    no2SensorReady = false;
  }

  if (currentSerialNumber.length() == 12) {
    if (currentSerialNumber != no2SerialNumber) {
      sp("[NO2] Sensor Seems to be changed! The Serial Number in memory is '");
      sp(no2SerialNumber);
      sp("' while the new one is '");
      sp(currentSerialNumber);
      spln("'. Writing change to memory and resetting Zeroing Information.");
      no2SerialNumber = currentSerialNumber;
      preferences.begin("klimerko", false);
      preferences.putString(preferences_no2SerialNumber, no2SerialNumber);
      preferences.putString(preferences_no2LastZeroing, preferences_LastZeroingDefault);
      preferences.putString(preferences_no2LastFailedZeroing, preferences_LastFailedZeroingDefault);
      preferences.end();
      publishMetadata();
      // Reset the averages since it's a new sensor
      avgNo2.reset();
      avgNo2Temp.reset();
      avgNo2Hum.reset();
    }
  }

  sp("[NO2 PARSED] S/N: ");
  sp(no2SerialNumber);
  sp(", Conc (ug/m3): ");
  sp(no2CurrentConcentration);
  sp(", Conc Avg (ug/m3): ");
  sp(no2AverageConcentration);
  sp(", Conc (PPB): ");
  sp(no2CurrentConcentrationPPB);
  sp(", ADC: ");
  sp(no2CurrentConcentrationADC);
  sp(", Temp: ");
  sp(no2CurrentTemperature);
  sp(", Temp Avg: ");
  sp(no2AverageTemperature);
  sp(", Hum: ");
  sp(no2CurrentHumidity);
  sp(", Hum Avg: ");
  sp(no2AverageHumidity);
  sp(", TempDigital: ");
  sp(no2CurrentTemperatureDigital);
  sp(", HumDigital: ");
  sp(no2CurrentHumidityDigital);
  sp(", Online Since: ");
  sp(no2SensorUptime_Days);
  sp(":");
  sp(no2SensorUptime_Hours);
  sp(":");
  sp(no2SensorUptime_Minutes);
  sp(":");
  spln(no2SensorUptime_Seconds);

  return true;
}

bool readPMS() {
  pms.read();
  if (pms) {
    if (pmsSensorRetryNumber >= sensorRetriesBeforeConsideredOffline && !pmsSensorOnline) {
      spln("[PMS] The sensor is back online!");
      pmsSensorOnline = true;
      publishMetadata();
    }

    pm1Current   = pms.pm01;
    pm2_5Current = pms.pm25;
    pm10Current  = pms.pm10;

    pm1Average   = avgPM1.reading(pm1Current);
    pm2_5Average = avgPM25.reading(pm2_5Current);
    pm10Average  = avgPM10.reading(pm10Current);

    sp("[PMS] PM 1: ");
    sp(pm1Current);
    sp(", PM 1 Avg: ");
    sp(pm1Average);
    sp(", PM 2.5: ");
    sp(pm2_5Current);
    sp(", PM 2.5 Avg: ");
    sp(pm2_5Average);
    sp(", PM 10: ");
    sp(pm10Current);
    sp(", PM 10 Avg: ");
    spln(pm10Average);

    pmsSensorRetryNumber = 0; // Must be here in case the sensor reconnects before its considered offline
    pmsSensorOnline = true;
    return true;
  } else { // Something went wrong
    pmsSensorRetryNumber++;
    sp("[PMS] Couldn't get data this time. ");
    sp(pmsSensorRetryNumber);
    sp("/");
    sp(sensorRetriesBeforeConsideredOffline);
    sp(", Reason: ");

    switch (pms.status) {
      case pms.OK: // Should never come here
        break;
      case pms.ERROR_TIMEOUT:
        spln(F(PMS_ERROR_TIMEOUT));
        break;
      case pms.ERROR_MSG_UNKNOWN:
        spln(F(PMS_ERROR_MSG_UNKNOWN));
        break;
      case pms.ERROR_MSG_HEADER:
        spln(F(PMS_ERROR_MSG_HEADER));
        break;
      case pms.ERROR_MSG_BODY:
        spln(F(PMS_ERROR_MSG_BODY));
        break;
      case pms.ERROR_MSG_START:
        spln(F(PMS_ERROR_MSG_START));
        break;
      case pms.ERROR_MSG_LENGTH:
        spln(F(PMS_ERROR_MSG_LENGTH));
        break;
      case pms.ERROR_MSG_CKSUM:
        spln(F(PMS_ERROR_MSG_CKSUM));
        break;
      case pms.ERROR_PMS_TYPE:
        spln(F(PMS_ERROR_PMS_TYPE));
        break;
      }

    if (pmsSensorRetryNumber >= sensorRetriesBeforeConsideredOffline) {
      spln("[PMS] The sensor seems to be offline!");
      if (pmsSensorOnline) {
        // Reset the averages since we don't know how long the sensor was offline
        spln("[PMS] Averaging Data Reset Because Sensor is Offline.");
        avgPM1.reset();
        avgPM25.reset();
        avgPM10.reset();
        pmsSensorOnline = false;
        publishMetadata();
      }
    }

    return false;
  }
}

bool zeroSO2() {
  String commandString;
  spln("[SO2] Zeroing Sensor...");
  unsigned long sensorSerialWaitStart = millis();
  while (so2Serial.available()) {
    so2Serial.read();
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("[SO2] Zeroing Failed. Sensor didn't reply in time.");
      return false;
    }
  }
  so2Serial.flush();
  so2Serial.write('Z');
  // Should give "\r\nSetting zero... done\r\n"
  sensorSerialWaitStart = millis();
  while (!so2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("[SO2] Zeroing Failed. Sensor didn't reply in time.");
      return false;
    }
  }
  commandString = so2Serial.readStringUntil('\n');
  spln(commandString);
  delay(10);
  sensorSerialWaitStart = millis();
  while (!so2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("[SO2] Zeroing Failed. Sensor didn't reply in time.");
      return false;
    }
  }
  commandString = so2Serial.readStringUntil('\n');
  spln(commandString); 
  
  if (commandString == "Setting zero...done\r") {
    spln("[SO2] Sensor Succesfully Zeroed! Averaging Values Reset.");
    // Reset the averaging since the values are going to be different now
    avgSo2.reset();
    avgSo2Temp.reset();
    avgSo2Hum.reset();
    return true;
  } else {
    spln("[SO2] Sensor Zeroing FAILED!");
  }
  return false;
}

bool zeroNO2() {
  String commandString;
  spln("[NO2] Zeroing Sensor...");
  unsigned long sensorSerialWaitStart = millis();
  while (no2Serial.available()) {
    no2Serial.read();
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("[NO2] Zeroing Failed. Sensor didn't reply in time.");
      return false;
    }
  }
  no2Serial.flush();
  no2Serial.write('Z');
  // Should give "\r\nSetting zero... done\r\n"
  sensorSerialWaitStart = millis();
  while (!no2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("[NO2] Zeroing Failed. Sensor didn't reply in time.");
      return false;
    }
  }
  commandString = no2Serial.readStringUntil('\n');
  spln(commandString);
  delay(10);
  sensorSerialWaitStart = millis();
  while (!no2Serial.available()) {
    if (millis() - sensorSerialWaitStart >= sensorSerialWaitTime) {
      spln("[NO2] Zeroing Failed. Sensor didn't reply in time.");
      return false;
    }
  }
  commandString = no2Serial.readStringUntil('\n');
  spln(commandString); 
  
  if (commandString == "Setting zero...done\r") {
    spln("[NO2] Sensor Succesfully Zeroed! Averaging Values Reset.");
    // Reset the averaging since the values are going to be different now
    avgNo2.reset();
    avgNo2Temp.reset();
    avgNo2Hum.reset();
    return true;
  } else {
    spln("[NO2] Sensor Zeroing FAILED!");
  }
  return false;
}

void zeroSensors(String sensor) {
  if (sensor == "SO2") {
    if (zeroSO2()) {
      so2LastZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_so2LastZeroing, so2LastZeroing);
      preferences.end();
    } else {
      so2LastFailedZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_so2LastFailedZeroing, so2LastFailedZeroing);
      preferences.end();
    }
  } else if (sensor == "NO2") {
    if (zeroNO2()) {
      no2LastZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_no2LastZeroing, no2LastZeroing);
      preferences.end();
    } else {
      no2LastFailedZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_no2LastFailedZeroing, no2LastFailedZeroing);
      preferences.end();
    }
  } else if (sensor == "ALL") {
    if (zeroSO2()) {
      so2LastZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_so2LastZeroing, so2LastZeroing);
      preferences.end();
    } else {
      so2LastFailedZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_so2LastFailedZeroing, so2LastFailedZeroing);
      preferences.end();
    }
    if (zeroNO2()) {
      no2LastZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_no2LastZeroing, no2LastZeroing);
      preferences.end();
    } else {
      no2LastFailedZeroing = timeClient.getFormattedDate();
      preferences.begin("klimerko", false);
      preferences.putString(preferences_no2LastFailedZeroing, no2LastFailedZeroing);
      preferences.end();
    }
  } else {
    spln("Wrong parameter used for 'zeroSensor(String)'");
    return;
  }

  spln("Zeroing Data Written to Persistant Storage.");
  publishMetadata();
}

void publishSensorData() {
  char JSONmessageBuffer[2048];
  DynamicJsonDocument doc(2048);
  doc["sent_at"] = timeClient.getFormattedDate();
  doc["client_id"] = MQTT_CLIENT_ID;
  JsonObject data = doc.createNestedObject("data");

  if (so2SensorOnline) {
    data["SO2"]             = so2AverageConcentration;
    data["so2_adc"]         = so2CurrentConcentrationADC;
    data["so2_ready"]       = so2SensorReady;
  } else {
    spln("[DATA] Won't publish SO2 data since the sensor is offline.");
  }

  if (no2SensorOnline) {
    data["NO2"]             = no2AverageConcentration;
    data["no2_adc"]         = no2CurrentConcentrationADC;
    data["no2_ready"]       = no2SensorReady;
  } else {
    spln("[DATA] Won't publish NO2 data since the sensor is offline.");
  }

  if (pmsSensorOnline) {
    data["PM1"]             = pm1Average;
    data["PM2_5"]           = pm2_5Average;
    data["PM10"]            = pm10Average;
  } else {
    spln("[DATA] Won't publish PMS data since the sensor is offline.");
  }

  if (no2SensorOnline) {
    data["temperature"]     = no2AverageTemperature;
    data["humidity"]        = no2AverageHumidity;
  } else if (so2SensorOnline) {
    spln("[DATA] Using SO2 Temperature & Humidity data because NO2 is offline.");
    data["temperature"]     = so2AverageTemperature;
    data["humidity"]        = so2AverageHumidity;
  } else {
    spln("[DATA] Won't publish Temperature & Humidity data - both SO2 and NO2 are offline.");
  }

  sp("[DATA] Sending Sensor Data: ");
  serializeJson(doc, JSONmessageBuffer);
  serializeJson(doc, Serial);
  spln("");

  // v1.devices.{deviceId}.actions.ingest
  char topic[128];
  snprintf(topic, sizeof topic, "%s%s%s", "v1/devices/", MQTT_CLIENT_ID, "/actions/ingest");

  if (mqtt.publish(topic, JSONmessageBuffer, true)) {
    spln("[MQTT] Sensor data sent!");
  } else {
    spln("[MQTT] Sensor data failed to send.");
  }
}

void publishSensorDataLoop() {
  publishSensorDataLoopCurrentTime = millis();
  if (millis() - sensorDataLastRead >= sensorDataReadInterval*1000) {
    spln("[DATA] Reading Sensor Data...");
    if (so2SensorOnline) { // Read the sensor if it's online. If it's considered offline, fallback to less frequent reading (just to check if it has been connected)
      readSO2();
    } else if (publishSensorDataLoopCurrentTime - so2SensorLastRecoveryAttemptTime >= sensorDataReadIntervalWhenConsideredOffline * 1000) {
      spln("[SO2] Now checking for availability");
      readSO2();
      if (!so2SensorOnline) {
        sp("[SO2] Will check for availability again in ");
        sp(sensorDataReadIntervalWhenConsideredOffline);
        spln(" seconds.");
      }
      so2SensorLastRecoveryAttemptTime = publishSensorDataLoopCurrentTime;
    }

    if (no2SensorOnline) { // Read the sensor if it's online. If it's considered offline, fallback to less frequent reading (just to check if it has been connected)
      readNO2();
    } else if (publishSensorDataLoopCurrentTime - no2SensorLastRecoveryAttemptTime >= sensorDataReadIntervalWhenConsideredOffline * 1000) {
      spln("[NO2] Now checking for availability");
      readNO2();
      if (!no2SensorOnline) {
        sp("[NO2] Will check for availability again in ");
        sp(sensorDataReadIntervalWhenConsideredOffline);
        spln(" seconds.");
      }
      no2SensorLastRecoveryAttemptTime = publishSensorDataLoopCurrentTime;
    }

    if (pmsSensorOnline) { // Read the sensor if it's online. If it's considered offline, fallback to less frequent reading (just to check if it has been connected)
      readPMS();
    } else if (publishSensorDataLoopCurrentTime - pmsSensorLastRecoveryAttemptTime >= sensorDataReadIntervalWhenConsideredOffline * 1000) {
      spln("[PMS] Now checking for availability");
      readPMS();
      if (!pmsSensorOnline) {
        sp("[PMS] Will check for availability again in ");
        sp(sensorDataReadIntervalWhenConsideredOffline);
        spln(" seconds.");
      }
      pmsSensorLastRecoveryAttemptTime = publishSensorDataLoopCurrentTime;
    }
    sensorDataLastRead = publishSensorDataLoopCurrentTime;
  }
  publishSensorDataLoopCurrentTime = millis();
  if (millis() - sensorDataLastPublish >= sensorDataPublishInterval*1000) {
    spln("[DATA] Publishing Sensor Data...");
    publishSensorData();
    sensorDataLastPublish = publishSensorDataLoopCurrentTime;
  }
}

void setSensorDataPublishInterval(int interval) {
  if (interval >= sensorDataPublishIntervalMin && interval <= sensorDataPublishIntervalMax) {
    sensorDataPublishInterval = interval;
    preferences.begin("klimerko", false);
    preferences.putInt(preferences_sensorDataPublishInterval, sensorDataPublishInterval);
    preferences.end();
    sp("Sensor Data Publishing Interval Changed to: ");
    sp(sensorDataPublishInterval);
    spln(" seconds. Saved in persistant memory.");
    publishMetadata();
  } else {
    sp("Failed to set new Sensor Data Publishing Interval. The argument '");
    sp(interval);
    sp("' is not within range (");
    sp(sensorDataPublishIntervalMin);
    sp(" - ");
    sp(sensorDataPublishIntervalMax);
    spln(" seconds)");
  }
}

// ----------------------------------------------------------------------------------------------

void firmwareUpdateStarted() {
  rgb[0] = CRGB::Magenta;
  FastLED.show();
}

void firmwareUpdateProgress(int current, int total) {
  esp_task_wdt_reset(); // Reset the watchdog so it doesn't reboot the device
  sp("[OTA] Firmware Update: ");
  sp(current);
  sp(" of ");
  sp(total);
  spln(" bytes...");
  // Flash the RGB LED in Magenta when updating firmware
  if (rgbOtaSwitch) {
    rgb[0] = CRGB::Magenta;
    FastLED.show();
    rgbOtaSwitch = !rgbOtaSwitch;
  } else {
    rgb[0] = CRGB::Black;
    FastLED.show();
    rgbOtaSwitch = !rgbOtaSwitch;
  }
}

void firmwareUpdateFinished() {
  spln("[OTA] Firmware Update Finished Successfully! Now resetting Klimerko.");
  preferences.begin("klimerko", false);
  preferences.putString(preferences_lastSuccessfulOTA, timeClient.getFormattedDate());
  preferences.end();
}

void firmwareUpdateError(int error) {
  sp("[OTA] Firmware Update Fatal Error: ");
  spln(error);
  preferences.begin("klimerko", false);
  preferences.putString(preferences_lastFailedOTA, timeClient.getFormattedDate());
  preferences.end();
}

void firmwareUpdate(bool forced) { // Using argument "true" will force a firmware update - will not check firmware version and TLS
  WiFiClientSecure firmwareNetworkClient;
  if (forced) {
    spln("[OTA] Running Forced Firmware Update... >>>> DO NOT POWER OFF THE DEVICE <<<<");
    firmwareNetworkClient.setInsecure();
  } else {
    spln("[OTA] Running Firmware Update... >>>> DO NOT POWER OFF THE DEVICE <<<<");
    firmwareNetworkClient.setCACert(fwRootCACertificate);
  }
  httpUpdate.onStart(firmwareUpdateStarted);
  httpUpdate.onEnd(firmwareUpdateFinished);
  httpUpdate.onProgress(firmwareUpdateProgress);
  httpUpdate.onError(firmwareUpdateError);
  httpUpdate.rebootOnUpdate(true);
  esp_task_wdt_reset(); // Reset the watchdog timer so the device doesn't reboot
  t_httpUpdate_return ret = httpUpdate.update(firmwareNetworkClient, firmwareUpdateFirmwareURL);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    sp("[OTA] HTTP_UPDATE_FAILD Error (");
    sp(httpUpdate.getLastError());
    sp("): ");
    spln(httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    spln("[OTA] HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    spln("[OTA] HTTP_UPDATE_OK");
    break;
  }
}

bool firmwareUpdateCheck() {
  String  payload;
  int     httpCode;
  String  url = "";
  url += firmwareUpdateFirmwareVersionURL;
  url += "?";
  url += String(rand());
  sp("[OTA] Checking for newer firmware using ");
  spln(url);
  WiFiClientSecure firmwareNetworkClient;

  firmwareNetworkClient.setCACert(fwRootCACertificate);
  HTTPClient https;
  if (https.begin(firmwareNetworkClient, url)) { // HTTPS      
    delay(100);
    httpCode = https.GET();
    delay(100);
    if (httpCode == HTTP_CODE_OK) { // if version received
      payload = https.getString(); // save received version
    } else {
      sp("[OTA] Error in downloading version file: ");
      spln(httpCode);
    }
    https.end();
  }

  if (httpCode == HTTP_CODE_OK) { // if version received
    payload.trim();
    if (payload.equals(firmwareVersion)) {
      spf("[OTA] Device already on latest firmware version: %s\n", firmwareVersion);
      return false;
    } else {
      spln(payload);
      spln("[OTA] New firmware detected!");
      return true;
    }
  } 
  return false;  
}

void firmwareUpdateLoop() {
  if (millis() - firmwareUpdateLastCheck >= firmwareUpdateCheckInterval * 1000) {
    if (firmwareUpdateCheck()) {
      firmwareUpdate(false); // Update the firmware normally (not forced)
    }
    firmwareUpdateLastCheck = millis();
  }
}

void wifiConfigSaveMqtt() { // Called when user saves MQTT Credentials using WiFi Configuration portal
  sp("User is adding new MQTT Password: ");
  spln(portalMqttPassword.getValue());
  strcpy(MQTT_PASSWORD, portalMqttPassword.getValue());
  preferences.begin("klimerko", false);
  preferences.putString("mqtt_password", MQTT_PASSWORD);
  preferences.end();
  sp("MQTT Password Written to Persistant Storage: ");
  spln(MQTT_PASSWORD);
}

void rgbControlLoop() 
{
  if (wm.getConfigPortalActive() && !rgbEffect_WiFiConfig && !rgbEffect_GreenBlink) {
    rgbEffect_WiFiConfig = true;
    if (rgbEffect_WiFi) {
      rgbEffect_WiFiExpire = true;
    }
    if (rgbEffect_Mqtt) {
      rgbEffect_MqttExpire = true;
    }
    rgb[0].b = 255;
    FastLED.show();
  } else if ((rgbEffect_GreenBlink && rgbEffect_WiFiConfig) || (!wm.getConfigPortalActive() && rgbEffect_WiFiConfig)) {
    rgbEffect_WiFiConfig = false;
    rgb[0].b = 0;
    FastLED.show();
  }

  if (wifiConnectionLost && !rgbEffect_WiFi && !rgbEffect_WiFiConfig && !rgbEffect_GreenBlink) {
    rgbEffect_WiFi = true;
  } else if ((rgbEffect_GreenBlink && rgbEffect_WiFi) || (!wifiConnectionLost && rgbEffect_WiFi)) {
    rgbEffect_WiFiExpire = true;
  }

  if (mqttConnectionLost && !rgbEffect_Mqtt && !rgbEffect_WiFi && !rgbEffect_WiFiConfig && !rgbEffect_GreenBlink) {
    rgbEffect_Mqtt = true;
  } else if ((rgbEffect_WiFi && rgbEffect_Mqtt) || (rgbEffect_GreenBlink && rgbEffect_Mqtt) || (!mqttConnectionLost && rgbEffect_Mqtt)) { // set rgbEffectMqtt to false once rgbEffectMqttExpire is done
    rgbEffect_MqttExpire = true;
  }
}

void rgbLoop() {
  rgbControlLoop();

  if (rgbEffect_GreenBlink) {
    if (millis() - rgbEffect_GreenBlink_PrevTime >= rgbEffect_GreenBlink_Interval) {
      if (rgbEffect_GreenBlink_FadingUp) {
        rgb[0].g = 255;
        FastLED.show();
        rgbEffect_GreenBlink_FadingUp = false;
      } else {
        rgb[0].g = 0;
        FastLED.show();
        rgbEffect_GreenBlink_FadingUp = true;
      }
      if (rgbEffect_GreenBlink_Count >= rgbEffect_GreenBlink_MaxBlinks) {
        rgbEffect_GreenBlink_Count = 0;
        rgbEffect_GreenBlink = false;
        rgb[0].g = 0;
        FastLED.show();
        rgbEffect_GreenBlink_FadingUp = true;
      } else {
        rgbEffect_GreenBlink_Count++;
      }
      rgbEffect_GreenBlink_PrevTime = millis();
    }
  }

  if (rgbEffect_WiFi || rgbEffect_WiFiExpire) {
    if (millis() - rgbEffect_WiFi_PrevTime >= rgbEffect_WiFi_Interval) {
      if (rgbEffect_WiFi_FadingUp) {
        rgbEffect_WiFi_FadeLevel = 255;
      } else {
        rgbEffect_WiFi_FadeLevel = 0;
      }
      if (rgbEffect_WiFiExpire) {
        rgbEffect_WiFi = false;
        rgbEffect_WiFiExpire = false; // Expired.
        rgbEffect_WiFi_FadeLevel = 0;
      }
      rgb[0].r = rgbEffect_WiFi_FadeLevel;
      FastLED.show();

      if (rgbEffect_WiFi_FadeLevel >= 255) {
        rgbEffect_WiFi_FadingUp = false;
      } else if (rgbEffect_WiFi_FadeLevel <= 0) {
        rgbEffect_WiFi_FadingUp = true;
      }
      rgbEffect_WiFi_PrevTime = millis();
    }
  }

  if (rgbEffect_Mqtt || rgbEffect_MqttExpire) {
    if (millis() - rgbEffect_Mqtt_PrevTime >= rgbEffect_Mqtt_Interval) {
      if (rgbEffect_Mqtt_FadingUp) {
        rgbEffect_Mqtt_FadeLevel = 255;
        rgbEffect_Mqtt_FadeLevelGreen = 100;
      } else {
        rgbEffect_Mqtt_FadeLevel = 0;
        rgbEffect_Mqtt_FadeLevelGreen = 0;
      }
      if (rgbEffect_MqttExpire) {
        rgbEffect_Mqtt = false;
        rgbEffect_MqttExpire = false; // Expired.
        rgbEffect_Mqtt_FadeLevel = 0;
        rgbEffect_Mqtt_FadeLevelGreen = 0;
      }
      rgb[0].r = rgbEffect_Mqtt_FadeLevel;
      rgb[0].g = rgbEffect_Mqtt_FadeLevelGreen; // So it becomes yellow
      FastLED.show();

      if (rgbEffect_Mqtt_FadeLevel >= 100) {
        rgbEffect_Mqtt_FadingUp = false;
      } else if (rgbEffect_Mqtt_FadeLevel <= 0) {
        rgbEffect_Mqtt_FadingUp = true;
      }
      rgbEffect_Mqtt_PrevTime = millis();
    }
  }
}

void wifiConfigEraseCredentials() {
  wm.resetSettings();
  spln("ERASED WiFi CREDENTIALS! Now rebooting the device.");
  wm.reboot();
}

void wifiConfigOtaStarted() { // Called before an OTA update is started using WiFi Configuration Portal
  esp_task_wdt_reset(); // Reset the watchdog timer so the device doesn't reboot
  rgb[0] = CRGB::Magenta;
  FastLED.show();
}

void wifiConfigStarted(WiFiManager *wm) { // Called once WiFi Configuration Portal is started
  wifiConfigActive = true;
  wifiConfigActiveSince = millis();
}

void wifiConfigStart () { // Starts WiFi Configuration Portal
  if (!wm.getConfigPortalActive()) {
    spln("Entering WiFi Configuration Mode...");
    wm.startConfigPortal(wifiConfigPortalSSID, wifiConfigPortalPassword);
    // TODO?: Turn on BLUE LED to indicate WiFi Configuration Portal
  } else {
    spln("WiFi Configuration Mode is already active!");
  }
}

void wifiConfigStop() { // Stops WiFi Configuration Portal
  if (wm.getConfigPortalActive()) {
    wm.stopConfigPortal();
    wifiConfigActive = false;
    spln("Stopped WiFi Configuration Portal");
  } else {
    spln("Can't stop WiFi Configuration Portal because it's not running.");
  }
}

void wifiConfigLoop() {
  // Keep web portal in the loop if it's supposed to be active
  if (wm.getConfigPortalActive()) {
    wm.process();
    // if (millis() - wifiConfigActiveSince >= wifiConfigTimeout*1000) {
    //   spln("Stopping WiFi Configuration Mode Because of Inactivity.");
    //   wifiConfigStop();
    //   // TODO: Turn off BLUE LED to indicate WiFi Configuration Portal is now off
    // }
  }
}

void wifiConfigWebServerStarted() { // This can handle page requests on WiFi Configuration Portal
  wm.server->on("/exit", wifiConfigStop); // If user presses Exit, turn off WiFi Configuration Portal.
}

void wifiConfigButton() {
  wifiConfigButtonCurrentState = digitalRead(WIFI_CONFIG_PIN);
  if (wifiConfigButtonLastState == HIGH && wifiConfigButtonCurrentState == LOW) {
    wifiConfigButtonPressedTime = millis();
    wifiConfigButtonPressed = true;
    wifiConfigButtonLongPressDetected = false;
    // Button is being pressed at the moment
  } else if (wifiConfigButtonLastState == LOW && wifiConfigButtonCurrentState == HIGH) {
    wifiConfigButtonReleasedTime = millis();
    wifiConfigButtonPressed = false;
    long wifiConfigButtonPressDuration = wifiConfigButtonReleasedTime - wifiConfigButtonPressedTime;
    if (wifiConfigButtonPressDuration > wifiConfigButtonShortPressTime && wifiConfigButtonPressDuration < wifiConfigButtonLongPressTime) {
      spln("WiFi Configuration Button Short Press Detected!");
      wifiConfigStop();
    }
  }

  if (wifiConfigButtonPressed && !wifiConfigButtonLongPressDetected) {
    if (millis() - wifiConfigButtonPressedTime > wifiConfigButtonLongPressTime) {
      wifiConfigButtonLongPressDetected = true;
      spln("WiFi Configuration Button Long Press Detected!");
      wifiConfigStart();
    }
  }
  wifiConfigButtonLastState = wifiConfigButtonCurrentState;
}

void mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length) {
  //concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }

  sp("[MQTT] Received Message '");
  sp(payload);
  sp("' on topic '");
  sp(p_topic);
  spln("' ");

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    sp("[MQTT] deserializeJson() failed: ");
    spln(error.f_str());
    return;
  }

  if (doc["type"] == "device_config") {
    if (doc["data"]["zero_sensors"] == true) {
      zeroSensors("ALL");
    }
    if (doc["data"]["zero_so2"] == true) {
      zeroSensors("SO2");
    }
    if (doc["data"]["zero_no2"] == true) {
      zeroSensors("NO2");
    }
    if (doc["data"]["erase_wifi_credentials"] == true) {
      wifiConfigEraseCredentials();
    }
    if (doc["data"]["reboot_device"] == true) {
      spln("Rebooting the device now...");
      wm.reboot();
    }
    if (doc["data"]["erase_zeroing_data"] == true) {
      preferences.begin("klimerko", false);
      preferences.putString(preferences_no2LastZeroing, preferences_LastZeroingDefault);
      preferences.putString(preferences_no2LastFailedZeroing, preferences_LastFailedZeroingDefault);
      preferences.putString(preferences_so2LastZeroing, preferences_LastZeroingDefault);
      preferences.putString(preferences_so2LastFailedZeroing, preferences_LastFailedZeroingDefault);
      preferences.end();
      so2LastZeroing = preferences_LastZeroingDefault;
      so2LastFailedZeroing = preferences_LastFailedZeroingDefault;
      no2LastZeroing = preferences_LastZeroingDefault;
      no2LastFailedZeroing = preferences_LastFailedZeroingDefault;
      spln("[Persistant Storage] Zeroing Data Erased from Persistant Storage!");
      publishMetadata();
    }
    if (doc["data"]["sensor_publishing_interval"]) {
      spln("Received message to set sensor data publishing interval, but this feature is disabled because of sensor averaging.");
      //setSensorDataPublishInterval(doc["data"]["sensor_publishing_interval"]);
    }
    if (doc["data"]["identify_device"] == true) {
      spln("Blinking the LED Green to Identify Device (Same green flash as when device is connected)...");
      rgbEffect_GreenBlink = true;
    }
    if (doc["data"]["force_ota_update"] == true) {
      spln("Forcing the download & installation of the newest firmware available for Klimerko Pro...");
      firmwareUpdate(true); // Force the firmware update
    }
  }
}

void mqttSubscribeTopics() {
  char eventTopic[128];
  // snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceCreds->getDeviceId(), "/state");
  snprintf(eventTopic, sizeof eventTopic, "%s%s%s", "v1/devices/", MQTT_CLIENT_ID, "/events");
  mqtt.subscribe(eventTopic);
  sp("[MQTT] Subscribed to topic: ");
  spln(eventTopic);
}

bool connectMQTT() { // Connects to MQTT
  if (!wifiConnectionLost) {
    sp("[MQTT] Connecting to ");
    sp(MQTT_SERVER);
    sp(" as '");
    sp(MQTT_USERNAME);
    // sp("' using password '");
    // sp(MQTT_PASSWORD);
    spln("'");
    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      spln("[MQTT] Connected!");
      mqttSubscribeTopics();
      if (mqttConnectionLost) {
        // TODO?: Turn off LED
        mqttConnectionLost = false;
        publishMetadata();
      }
      rgbEffect_GreenBlink = true;
      return true;
    } else {
      sp("[MQTT] Connection Failed, Reason: ");
      spln(mqtt.state());
      return false;
    }
  }
  return false;
}

void maintainMQTT() { // Reconnects to MQTT if connection lost, otherwise loops MQTT client
  if (mqtt.connected()) {
    if (mqttConnectionLost) {
      mqttConnectionLost = false; // Redundant because it's set in connectMQTT function if it succeeds
      rgbEffect_GreenBlink = true;
    }
    mqtt.loop();
  } else {
    if (!mqttConnectionLost) {
      spln("[MQTT] Lost Connection...");
      mqttConnectionLost = true;
    }
    if (millis() - mqttReconnectLastAttempt >= mqttReconnectInterval * 1000) {
      connectMQTT();
      mqttReconnectLastAttempt = millis();
    }
  }
}

bool initMQTT() { // Sets client encryption (TLS), initializes and connects to MQTT
  //networkClient.setTimeout(3); // Questionable
  // TLS 1.2 Encryption
  // networkClient.setCACert(mqttRootCACertificate);
  // networkClient.setCertificate(MQTT_CLIENT_CERTIFICATE);
  // networkClient.setPrivateKey(MQTT_CLIENT_KEY);
  mqtt.setBufferSize(MQTT_MAX_MESSAGE_SIZE);
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  //mqtt.setKeepAlive(15); // Period after which (if no data was flowing from/to the device) klimerko will send a "check" message to broker
  //mqtt.setSocketTimeout(5);
  return connectMQTT();
}

bool connectWiFi() {
  if(!wm.autoConnect(wifiConfigPortalSSID, wifiConfigPortalPassword)) {
    spln("[WiFi] Failed to connect!");
    wifiConnectionLost = true;
    return false;
  } else {
    spln("[WiFi] Successfully Connected!");
    wifiConnectionLost = false;
    rgbEffect_GreenBlink = true;
    return true;
  }
}

void maintainWiFi() { // Reconnects to WiFi if Disconnected. If connected, loops Arduino OTA
  if (WiFi.status() == WL_CONNECTED) {
    if (wifiConnectionLost) {
      sp("[WiFi] Connection Re-Established! IP: ");
      spln(WiFi.localIP());
      wifiConnectionLost = false;
      rgbEffect_GreenBlink = true;
    }
  } else {
    if (!wifiConnectionLost) {
      spln("[WiFi] Connection Lost!");
      wifiConnectionLost = true;
    }
    if (millis() - wifiReconnectLastAttempt >= wifiReconnectInterval * 1000 && !wm.getConfigPortalActive()) {
      spln("[WiFi] Trying to reconnect...");
      connectWiFi();
      wifiReconnectLastAttempt = millis();
    }
  }
}

void initWifiConfig() {
  wm.setDebugOutput(true, "[WiFiConfig]"); // log line prefix, default "*wm:"
  wm.addParameter(&portalMqttPassword);
  wm.addParameter(&portalDisplayFirmwareVersion);
  wm.addParameter(&portalDisplayCredits);
  wm.setSaveParamsCallback(wifiConfigSaveMqtt);
  wm.setPreOtaUpdateCallback(wifiConfigOtaStarted);
  wm.setAPCallback(wifiConfigStarted);
  wm.setWebServerCallback(wifiConfigWebServerStarted);
  wm.setConfigPortalBlocking(false);
  wm.setConnectRetries(2);
  wm.setConnectTimeout(10);
  wm.setHostname("KLIMERKO-PRO");
  wm.setCountry("RS");
  wm.setEnableConfigPortal(false); // Don't open config portal if wifi fails at boot/initial connect
  wm.setParamsPage(true); // WEIRD
  //WiFi.printDiag(Serial);
  sp("Saved WiFi Network: ");
  spln((String)wm.getWiFiSSID());
  //spln("PASS: " + (String)wm.getWiFiPass());
  connectWiFi();
}

void initSensors() {
  initSO2();
  initNO2();
  pms.init();
  avgSo2.begin();
  avgSo2Temp.begin();
  avgSo2Hum.begin();
  avgNo2.begin();
  avgNo2Temp.begin();
  avgNo2Hum.begin();
  avgPM1.begin();
  avgPM25.begin();
  avgPM10.begin();
}

void initRGB() {
  FastLED.addLeds<WS2812B, RGB_PIN, GRB>(rgb, RGB_NUM_LEDS);
  for (int i=0; i<3; i++) {
    rgb[0] = CRGB::Green;
    FastLED.show();
    delay(100);
    rgb[0] = CRGB::Blue;
    FastLED.show();
    delay(100);
    rgb[0] = CRGB::Red;
    FastLED.show();
    delay(100);
    i++;
  }
  rgb[0] = CRGB::Black;
  FastLED.show();
}

String getResetReason() { // Get last reset reason
  switch (rtc_get_reset_reason(0))
  {
    case 1  : resetReason = "Vbat power on reset";break;
    case 3  : resetReason = "Software reset digital core";break;
    case 4  : resetReason = "Legacy watch dog reset digital core";break;
    case 5  : resetReason = "Deep Sleep reset digital core";break;
    case 6  : resetReason = "Reset by SLC module, reset digital core";break;
    case 7  : resetReason = "Timer Group0 Watch dog reset digital core";break;
    case 8  : resetReason = "Timer Group1 Watch dog reset digital core";break;
    case 9  : resetReason = "RTC Watch dog Reset digital core";break;
    case 10 : resetReason = "Instrusion tested to reset CPU";break;
    case 11 : resetReason = "Time Group reset CPU";break;
    case 12 : resetReason = "Software reset CPU";break;
    case 13 : resetReason = "RTC Watch dog Reset CPU";break;
    case 14 : resetReason = "for APP CPU, reseted by PRO CPU";break;
    case 15 : resetReason = "Reset when the vdd voltage is not stable";break;
    case 16 : resetReason = "RTC Watch dog reset digital core and rtc module";break;
    default : resetReason = "NO_MEAN";
  }
  return resetReason;
}

String mac2String(byte ar[]) {
  String s;
  for (byte i = 0; i < 6; ++i)
  {
    char buf[2];
    sprintf(buf, "%02X", ar[i]); // J-M-L: slight modification, added the 0 in the format for padding 
    s += buf;
    //if (i < 5) s += ':';
  }
  return s;
}

void generateKlimerkoID() {
  uint64_t eFuseMAC = ESP.getEfuseMac();
  klimerkoID = mac2String((byte*) &eFuseMAC);
  sp("[ID] Klimerko Pro ID: ");
  sp(klimerkoID);
  klimerkoID.toCharArray(MQTT_CLIENT_ID, sizeof(MQTT_CLIENT_ID));
  //sprintf(MQTT_CLIENT_ID, "%s", klimerkoID);
  sp(", MQTT Client ID: ");
  sp(MQTT_CLIENT_ID);
  MQTT_USERNAME = MQTT_CLIENT_ID;
  // sprintf(wifiConfigPortalSSID, "KLIMERKO-%" PRIu64, KLIMERKO_ID); // PRIu64 Required to parse uint64_t value
  String temporaryWifiConfigPortalSSID = "KLIMERKO-" + klimerkoID;
  //sprintf(wifiConfigPortalSSID, "KLIMERKO-%s", klimerkoID);
  temporaryWifiConfigPortalSSID.toCharArray(wifiConfigPortalSSID, sizeof(wifiConfigPortalSSID));
  sp(", Unique SSID: ");
  spln(wifiConfigPortalSSID);
}

void setup() {
  Serial.begin(115200);
  spln("");
  spln("-------------- Klimerko Pro --------------");
  sp("Firmware Version: ");
  spln(firmwareVersion);
  sp("Sensor Data Collected Every ");
  sp(sensorDataReadInterval);
  sp(" seconds and published every ");
  sp(sensorDataPublishInterval);
  spln(" seconds.");
  spln("Device & Firmware Designed, Developed and Maintained by Vanja Stanic");

  // Watchdog, to reset the device if it hangs (specifically when doing an OTA update)
  esp_task_wdt_init(wdtTimeout, true);
  esp_task_wdt_add(NULL);

  WiFi.mode(WIFI_STA);     // By default, ESP32 is STA+AP
  getResetReason();        // Get last reset reason
  sp("Reset Reason: ");
  spln(resetReason);
  readPersistantStorage(); // Read variables from persistant storage (MQTT Password, etc)
  initRGB();
  generateKlimerkoID();    // Generate Unique ID and SSID
  esp_task_wdt_reset(); // Reset the watchdog timer so the device doesn't reboot
  initSensors();
  initWifiConfig();        // Initialize WiFi Configuration Portal
  esp_task_wdt_reset(); // Reset the watchdog timer so the device doesn't reboot
  timeClient.begin();
  timeClient.update();
  esp_task_wdt_reset(); // Reset the watchdog timer so the device doesn't reboot
  initMQTT();
}

void loop() {
  esp_task_wdt_reset(); // Reset the watchdog timer so the device doesn't reboot
  if (!wifiConnectionLost) {
    timeClient.update();
    firmwareUpdateLoop();
    publishMetadataLoop();
    maintainMQTT();
  }
  maintainWiFi();
  rgbLoop();
  wifiConfigButton();
  wifiConfigLoop();
  if (!wm.getConfigPortalActive()) {
    publishSensorDataLoop(); // Don't read and publish sensor data if WiFi Configuration Mode is active (hangs)
  }
}