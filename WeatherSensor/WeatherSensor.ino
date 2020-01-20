// WeatherSensor.ino - Richie Jarvis - richie@helkit.com
// Version: v0.0.3
// Github: https://github.com/richiejarvis/iotprojects/tree/master/WeatherSensor


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
#define CONFIG_VERSION "newval1"
#define STRING_LEN 128
#define NUMBER_LEN 32

// Variables
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
char sensorName[] = "weather";
const char wifiInitialApPassword[] = "weather1";
const char* ntpServer = "pool.ntp.org";


char elasticUsernameForm[STRING_LEN] = "weather";
char elasticPassForm[STRING_LEN] = "password";
char elasticHostForm[STRING_LEN] = "jd.heliosuk.net";
char elasticPortForm[STRING_LEN] = "19200";
char elasticIndexForm[STRING_LEN] = "weather-alias";
char latForm[NUMBER_LEN] = "50.937";
char lngForm[NUMBER_LEN] = "-0.021";
char locationNameForm[STRING_LEN] = "Observatory";

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to build an AP. (E.g. in case of lost password)
#define CONFIG_PIN 12

//// -- Status indicator pin.
////      First it will light up (kept LOW), on Wifi connection it will blink,
////      when connected to the Wifi it will turn off (kept HIGH).
// Not currently used
#define STATUS_PIN 13

long prevTime = 0;
HTTPClient http;
float temperature;
int iteration = 0;

// -- Callback method declarations.
void configSaved();
bool formValidator();

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

// Setup the Form Value to Parameter
IotWebConf iotWebConf(sensorName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfParameter elasticUsername = IotWebConfParameter("Elasticsearch Username", "elasticUsername", elasticUsernameForm, STRING_LEN);
IotWebConfParameter elasticPass = IotWebConfParameter("Elasticsearch Password", "elasticPass", elasticPassForm, STRING_LEN);
IotWebConfParameter elasticHost = IotWebConfParameter("Elasticsearch Hostname", "elasticHost", elasticHostForm, STRING_LEN);
IotWebConfParameter elasticPort = IotWebConfParameter("Elasticsearch Port", "elasticPort", elasticPortForm, STRING_LEN);
IotWebConfParameter elasticIndex = IotWebConfParameter("Elasticsearch Index", "elasticIndex", elasticIndexForm, STRING_LEN);
IotWebConfParameter latValue = IotWebConfParameter("Float param", "latValue", latForm, NUMBER_LEN, "number", "e.g. 41.451", NULL, "step='0.001'");
IotWebConfParameter lngValue = IotWebConfParameter("Float param", "lngValue", lngForm, NUMBER_LEN, "number", "e.g. -23.712", NULL, "step='0.001'");
IotWebConfParameter locationName = IotWebConfParameter("Sensor Name/Location Name", "locationName", locationNameForm, STRING_LEN);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  /* Initialise the sensor */
  if (!bme.begin(0x77, &Wire)) {
    Serial.println("Not on 0x77 I2C address - checking 0x76");
    if (!bme.begin(0x76, &Wire)) {
      Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
      while (1) delay(10);
    }
  }
  bme_temp->printSensorDetails();
  bme_pressure->printSensorDetails();
  bme_humidity->printSensorDetails();

  iotWebConf.setupUpdateServer(&httpUpdater);

  //Setup Custom parameters
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
  String errorState = "NONE";
  // Sanity check to make sure we are not underwater, or in space!
  if (temperature < -40.00) {
    errorState = "ERROR: TEMPERATURE SENSOR MISREAD";
    if (pressure > 1100.00) {
      errorState = "ERROR: PRESSURE SENSOR MISREAD";
    }
  }

  // Getting time via NTP is expensive, and causes problems with the IotWifiConf library
  // For now, Calling it more frequently to see if it is easier to attach to the AP
  if (iteration == 100) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      Serial.println("NOT CONNECTED");
    } else {
      long nowTime = time(&now);
      //      String sensorName(sensorName);
      // Construct dataset to send
      String dataSet = "{\"@timestamp\":";
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
      dataSet += "\",\"location\":\"";
      dataSet += latForm;
      dataSet += ",";
      dataSet += lngForm;   
      dataSet += "\"";
      dataSet += "}";

    
      Serial.println("Request:" + dataSet);
      if (nowTime != prevTime)
      {
        String url = "http://";
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

        http.begin(url);
        Serial.print("URL:" + url);
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(dataSet);
        Serial.println("Response:" + String(httpCode));
      }
      // Store the time, so we don't send for a second
      prevTime = nowTime;
    }
    iteration = 0;
  }
  iteration++;

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
  s += "<title>WeatherStation</title></head><body>";
  s += "<ul>";
  s += "<p>";
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
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void configSaved()
{
  Serial.println("Configuration was updated.");
}

bool formValidator()
{
  Serial.println("Validating form.");
  bool valid = true;
  //
  //  int l = server.arg(stringParam.getId()).length();
  //  if (l < 3)
  //  {
  //    stringParam.errorMessage = "Please enter some values";
  //    valid = false;
  //  }

  return valid;
}
