# WeatherSensor
__Author:__ Richie Jarvis - richie@helkit.com
## Description
A simple ESP8266 or ESP32 compatible piece of code to read a BME280 sensor data, and send via wifi to an Elastic Stack.
## Features
1. Auto wifi configuration using https://github.com/prampec/IotWebConf
2. Elasticsearch configuration parameters and WiFi are stored in flash on-chip once configured.
2. Read from BME280 and compatible Temperature, Pressure and Humidity sensors via i2c
3. Send JSON with the following Elasticsearch mapping definition (v0.0.3+)
```
{
    "@timestamp": 1579552556,
    "pressure": 1041.9,
    "temperature": -0.09,
    "humidity": 83.1,
    "errorState": "NONE",
    "sensorName": "Outside1",
    "location": "52.937,-3.021"
  }
```


