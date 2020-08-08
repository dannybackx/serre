   W O R K   I N   P R O G R E S S
#
# Chicken House Hatch and Sensor Module
#
# esp32 version, (c) 2020 by Danny Backx
#
# Previous work started in 2016, 2017
#

This module is meant to power a hatch that protects your chickens against nightly predators
by closing the hatch at night.

The hatch design is assumed to have a magnet and two sensors, so the controller can feel
when to stop the motor. It gets its time from a RTC (real time clock) module or over Internet,
and can feed environmental data if equipped with a sensor module (BMP180).

Earlier attempts at this :
- with an Arduino Uno Wifi
- I had a somewhat working solution with an Arduino Mega and an ESP8266, but the esp-link software (which I'd already extended) was insecure, and no apparent support for fixing that.

This is what this software is based on :
- An ESP32 with a combination of Arduino drivers and ESP-IDF build platform
- my library to provide certificates (ACME) and a way to find the module via dynamic dns.

Input and output hardware :
- a simple L298N motor steering module (older code uses the old style Adafruit Motor Shield)
- a LDR (light dependent resistor) to detect day/night
- a BMP180 sensor to monitor temperature and air pressure
- two analog hall effect sensors (SS49E for instance) to detect door open/close
- two push buttons to force door open/close

The code can periodically feed environmental data to ThingSpeak, and to MQTT.

Internet queries for sunrise/sunset times are performed daily, so the door operates based on
that as well as sunlight. The queries require another patch for esp-link to allow for REST
queries with long results.

Some of the data required to make this work are private. The real secrets.h file that
I use is not made available publically. Take the secrets.h.sample and adapt it for your
environment (your wifi network name and password, etc.).

You can query all kinds of things like this :
  mosquitto_pub -h pi4 -t kippen -m /light/query
  mosquitto_pub -h pi4 -t kippen -m /boot/query
  mosquitto_pub -h pi4 -t kippen -m /BMP180/query

Libraries and components :
- acmeclient
- Adafruit_MCP9808
- Adafruit_Sensor
- arduinojson
- ftpclient
*** Timezone

- arduino
- esp_littlefs
- littlefs
