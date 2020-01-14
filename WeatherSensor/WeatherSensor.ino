#include <IotWebConf.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include "time.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <IotWebConf.h>

Adafruit_BME280 bme; // use I2C interface
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

const String url = "http://elastic:thibor@172.24.42.100:19200/weather-alias/_doc";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
const char thingName[] = "weatherNew";
const char wifiInitialApPassword[] = "weather";


String prevDataSet = "";
long prevTime = 0;
HTTPClient http;

// Variables
float temperature;




// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "myvalue"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
// Not currently used
//#define CONFIG_PIN D2

//// -- Status indicator pin.
////      First it will light up (kept LOW), on Wifi connection it will blink,
////      when connected to the Wifi it will turn off (kept HIGH).
// Not currently used
//#define STATUS_PIN LED_BUILTIN

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);


void setup() {
  Serial.begin(115200);
  /* Initialise the sensor */
  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    while (1) delay(10);
  }
  bme_temp->printSensorDetails();
  bme_pressure->printSensorDetails();
  bme_humidity->printSensorDetails();

  iotWebConf.setupUpdateServer(&httpUpdater);

  // -- Initializing the configuration.
  iotWebConf.init();

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() {
    iotWebConf.handleNotFound();
  });




  //  // We start by connecting to a WiFi network
  //  Serial.println();
  //  Serial.print("Connecting to ");
  //  Serial.println(ssid);
  //  WiFi.begin(ssid, password);
  //  while (WiFi.status() != WL_CONNECTED) {
  //    delay(500);
  //    Serial.print(".");
  //  }
  //  Serial.println("");
  //  Serial.println("WiFi connected");
  //  Serial.println("IP address: ");
  //  Serial.println(WiFi.localIP());



}



void loop() {

  iotWebConf.doLoop();
  bool checkWifi = (bool) iotWebConf.checkWifiConnection);
    bool checkWifi = true;

  if (checkWifi)) {
    sensors_event_t temp_event, pressure_event, humidity_event;
    bme_temp->getEvent(&temp_event);
    bme_pressure->getEvent(&pressure_event);
    bme_humidity->getEvent(&humidity_event);

    // Store the values in local vars
    float temperature = temp_event.temperature;
    float humidity = humidity_event.relative_humidity;
    float pressure = pressure_event.pressure;
    // initialise an empty String for later on
    String errorState = "NONE";
    // Sanity check to make sure we are not underwater, or in space!
    if (temperature < -40.00) {
      errorState = "ERROR: TEMPERATURE SENSOR MISREAD";
      if (pressure > 1100.00) {
        errorState = "ERROR: PRESSURE SENSOR MISREAD";
      }
    }

    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
    } else {
      long nowTime = time(&now);
      String dataSet = "{\"@timestamp\": " + String(nowTime) + ",\"pressure\": " + String(pressure) + ",\"temperature\": " + String(temperature) + ",\"humidity\": " + String(humidity) + ",\"errorstate\": \"" + errorState + "\"}";
      // We can inspect the data being sent over the Serial line, in the Arduino IDE.
      if (dataSet != prevDataSet && nowTime != prevTimeValue) {
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(dataSet);
        Serial.println("HTTPCode:" + String(httpCode) + " - " + dataSet);
        prevDataSet = dataSet;
        prevTimeValue = nowTime;
      }
    }
  }
}

/**
 * Handle web requests to "/" path.
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
  s += "<title>IotWebConf 04 Update Server</title></head><body>Hello world!";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}
