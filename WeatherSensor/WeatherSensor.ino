// WeatherSensor.ino - Richie Jarvis - richie@helkit.com
// Version: v0.1.1 - 2020-02-05
// Github: https://github.com/richiejarvis/iotprojects/tree/master/WeatherSensor
// Version History
// v0.0.1 - Initial Release
// v0.0.2 - Added ES params
// v0.0.3 - I2C address change tolerance & lat/long
// v0.0.4 - SSL support
// v0.1.0 - Display all the variables
// v0.1.1 - Store seconds since epoch, and increment as time passes to reduce ntp calls

#include <IotWebConf.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include "time.h"
#include <HTTPClient.h>

Adafruit_BME280 bme; // use I2C interface
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();
// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "v0.1.1"
#define STRING_LEN 128
#define NUMBER_LEN 32

// Variables
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
char sensorName[] = "WeatherSensor";
const char wifiInitialApPassword[] = "WeatherSensor";
const char* ntpServer = "pool.ntp.org";


char elasticPrefixForm[STRING_LEN] = "https://";
char elasticUsernameForm[STRING_LEN] = "weather";
char elasticPassForm[STRING_LEN] = "password";
char elasticHostForm[STRING_LEN] = "sensor.helkit.com";
char elasticPortForm[STRING_LEN] = "443";
char elasticIndexForm[STRING_LEN] = "weather-alias";
char latForm[NUMBER_LEN] = "50.0";
char lngForm[NUMBER_LEN] = "0.0";
char locationNameForm[STRING_LEN] = "NewSensor";

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to build an AP. (E.g. in case of lost password)
#define CONFIG_PIN 12

//// -- Status indicator pin.
////      First it will light up (kept LOW), on Wifi connection it will blink,
////      when connected to the Wifi it will turn off (kept HIGH).
// Not currently used
#define STATUS_PIN 13




// -- Callback method declarations.
void configSaved();
bool formValidator();

// DNS and Webserver Initialisation
HTTPClient http;
DNSServer dnsServer;
HTTPUpdateServer httpUpdater;
WebServer server(80);

// Setup the Form Value to Parameter
IotWebConf iotWebConf(sensorName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfParameter elasticPrefix = IotWebConfParameter("Elasticsearch URL Scheme:", "elasticPrefix", elasticPrefixForm, STRING_LEN);
IotWebConfParameter elasticUsername = IotWebConfParameter("Elasticsearch Username", "elasticUsername", elasticUsernameForm, STRING_LEN);
IotWebConfParameter elasticPass = IotWebConfParameter("Elasticsearch Password", "elasticPass", elasticPassForm, STRING_LEN, "password");
IotWebConfParameter elasticHost = IotWebConfParameter("Elasticsearch Hostname", "elasticHost", elasticHostForm, STRING_LEN);
IotWebConfParameter elasticPort = IotWebConfParameter("Elasticsearch Port", "elasticPort", elasticPortForm, STRING_LEN);
IotWebConfParameter elasticIndex = IotWebConfParameter("Elasticsearch Index", "elasticIndex", elasticIndexForm, STRING_LEN);
IotWebConfParameter latValue = IotWebConfParameter("Decimal Longitude", "latValue", latForm, NUMBER_LEN, "number", "e.g. 41.451", NULL, "step='0.001'");
IotWebConfParameter lngValue = IotWebConfParameter("Decimal Latitude", "lngValue", lngForm, NUMBER_LEN, "number", "e.g. -23.712", NULL, "step='0.001'");
IotWebConfParameter locationName = IotWebConfParameter("Sensor Name/Location Name", "locationName", locationNameForm, STRING_LEN);


long lastNtpTimeRead = 0;
long nowTime = 0;
long prevTime = 0;
float temperature = 0;
//int iteration = 0;
String errorState = "NONE";
long secondsSinceLastCheck = 0;
boolean ntpSuccess = false;
long lastReadClock = 0;



void setup() {
  Serial.begin(115200);
  debugOutput("Starting WeatherSensor " + (String)CONFIG_VERSION);

  /* Initialise the sensor */
  // This part tries I2C address 0x77 first, and then falls back to using 0x76.
  // If there is no I2C data with these addresses on the bus, then it reports a Fatal error, and stops
  if (!bme.begin(0x77, &Wire)) {
    debugOutput("Not on 0x77 I2C address - checking 0x76");
    if (!bme.begin(0x76, &Wire)) {
      debugOutput("FATAL: Could not find a valid BME280 sensor, check wiring!");
      while (1) delay(10);
    }
  }
  // Output Sensor details from the I2C bus
  bme_temp->printSensorDetails();
  bme_pressure->printSensorDetails();
  bme_humidity->printSensorDetails();

  iotWebConf.setupUpdateServer(&httpUpdater);

  //Setup Custom parameters
  iotWebConf.addParameter(&elasticPrefix);
  iotWebConf.addParameter(&elasticUsername);
  iotWebConf.addParameter(&elasticPass);
  iotWebConf.addParameter(&elasticHost);
  iotWebConf.addParameter(&elasticPort);
  iotWebConf.addParameter(&elasticIndex);
  iotWebConf.addParameter(&latValue);
  iotWebConf.addParameter(&lngValue);
  iotWebConf.addParameter(&locationName);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;


  // -- Initializing the configuration.
  iotWebConf.init();
  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() {
    iotWebConf.handleNotFound();
  });
}

void loop() {

  // Check for configuration changes
  iotWebConf.doLoop();
  // Grab time
  getNtpTime();
//  debugOutput("DEBUG: Sec:" + (String)secondsSinceLastCheck + " lastNtpTimeRead:" + (String)lastNtpTimeRead);

  if (ntpSuccess && nowTime > prevTime)
  {
    sampleAndSend();
    // Store the last time we sent, so we can check if we need to do it again
    prevTime = nowTime;
  }

  delay(50);

}


void sampleAndSend() {
  // Start a sample
  sensors_event_t temp_event, pressure_event, humidity_event;
  bme_temp->getEvent(&temp_event);
  bme_pressure->getEvent(&pressure_event);
  bme_humidity->getEvent(&humidity_event);
  // Store the values from the BME280 in local vars
  float temperature = temp_event.temperature;
  float humidity = humidity_event.relative_humidity;
  float pressure = pressure_event.pressure;
  // Store whether the sensor was connected
  // Sanity check to make sure we are not underwater, or in space!
  if (temperature < -40.00) {
    errorState = "ERROR: TEMPERATURE SENSOR MISREAD";
    if (pressure > 1100.00) {
      errorState = "ERROR: PRESSURE SENSOR MISREAD";
    }
  }
  // Build the dataset to send
  String dataSet = " {\"@timestamp\":";
  dataSet += String(nowTime);
  dataSet += ",\"pressure\":";
  dataSet += String(pressure);
  dataSet += ",\"temperature\":";
  dataSet += String(temperature);
  dataSet += ",\"humidity\":";
  dataSet += String(humidity);
  dataSet += ",\"errorState\": \"";
  dataSet += errorState;
  dataSet += "\",\"sensorName\":\"";
  dataSet += locationNameForm;
  dataSet += "\",\"firmwareVersion\":\"";
  dataSet += (String)CONFIG_VERSION;
  dataSet += "\",\"location\":\"";
  dataSet += latForm;
  dataSet += ",";
  dataSet += lngForm;
  dataSet += "\"";
  dataSet += "}";

  // Build the URL to send the JSON structure to
  String url = elasticPrefixForm;
  url += elasticUsernameForm;
  url += ":";
  url += elasticPassForm;
  url += "@";
  url += elasticHostForm;
  url += ":";
  url += elasticPortForm;
  url += "/";
  url += elasticIndexForm;
  url += "/_doc";
  debugOutput("INFO: Sent: " + dataSet);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(dataSet);
  debugOutput("INFO: Response: " + String(httpCode));
}

/**
   Handle web requests to "/" path.
*/
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }

  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>WeatherStation - " + (String)CONFIG_VERSION + "</title></head><body>";
  s += "<ul>";
  s += "<p>";
  s += "<li>Elasticsearch Scheme: ";
  s += elasticPrefixForm;
  s += "<li>Elasticsearch Username: ";
  s += elasticUsernameForm;
  s += "<li>Elasticsearch Password: ";
  s += elasticPassForm;
  s += "<li>Elasticsearch Hostname: ";
  s += elasticHostForm;
  s += "<li>Elasticsearch Port: ";
  s += elasticPortForm;
  s += "<li>Elasticsearch Index/Alias: ";
  s += elasticIndexForm;
  s += "<li>Sensor Latitude: ";
  s += latForm;
  s += "<li>Sensor Longitude: ";
  s += lngForm;
  s += "<li>SensorName: ";
  s += locationNameForm;
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";
  server.send(200, "text/html", s);
}

void configSaved()
{
  debugOutput("INFO: Configuration was updated.");
}

bool formValidator()
{

  bool valid = true;
  int countOfVars = server.args();
  debugOutput("INFO: Number of Variables Stored: " + String(countOfVars));
  // Check we have the right number of variables (15 at last count)
  if (countOfVars < 15)
  {
    debugOutput("ERROR: Form validation failed");
    valid = false;
  }
  return valid;
}

void getNtpTime()
{
  if (!ntpSuccess || secondsSinceLastCheck >= 600) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      debugOutput("ERROR: Cannot connect to NTP server");
      ntpSuccess = false;
    } else {
      nowTime = time(&now);
      lastNtpTimeRead = nowTime;
      lastReadClock = millis() / 1000;
      debugOutput("INFO: NTP Server Time Now: " + (String)nowTime);
      ntpSuccess = true;
      secondsSinceLastCheck = 0;
    }

  } else {  
    //How do we set nowTime to the current time?  We need to know how many seconds have elasped since the last ntp store.
    secondsSinceLastCheck = (millis() / 1000) - lastReadClock;
    nowTime = lastNtpTimeRead + secondsSinceLastCheck;
  }
}

void debugOutput(String textToSend)
{
  Serial.println("ts:" + (String)nowTime + " up:" + (String)(millis() / 1000) + ": " + textToSend);
}
