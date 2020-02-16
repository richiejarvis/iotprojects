# WeatherSensor
__Author:__ Richie Jarvis - richie@helkit.com
## Description
A simple ESP8266 or ESP32 compatible piece of code to read a BME280 sensor data, and send via wifi to an Elastic Stack.

![Wiring Diagram](https://github.com/richiejarvis/iotprojects/blob/master/WeatherSensor/ESP32-BME280.png)

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

Elasticsearch will normally detect these datatypes, however, if you wish to create a template, here is what I use:

```
PUT _template/weather_template
{
  "version": 2,
  "order": 0,
  "index_patterns": [
    "weather-*"
  ],
  "settings": {
    "index": {
      "lifecycle": {
        "name": "weather-ilm",
        "rollover_alias": "weather-alias"
      },
      "number_of_shards": "1",
      "number_of_replicas": "1"
    }
  },
  "mappings": {
    "properties": {
      "@timestamp": {
        "format": "epoch_second",
        "type": "date"
      },
      "errorState": {
        "type": "keyword"
      },
      "sensorName": {
        "type": "keyword"
      },
      "temperature": {
        "type": "float"
      },
      "humidity": {
        "type": "float"
      },
      "location": {
        "type": "geo_point"
      },
      "pressure": {
        "type": "float"
      }
    }
  }
}
```


