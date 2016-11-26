#
# Chicken House Hatch and Sensor Module
# Arduino Uno WiFi version
#
# (c) 2016 by Danny Backx
#

This module is meant to power a hatch that protects your chickens against nightly predators
by closing the hatch at night.

The hatch design is assumed to have a magnet and two sensors, so the controller can feel
when to stop the motor. It gets its time from a RTC (real time clock) module, and can
feed environmental data if equipped with a sensor module (BMP180).

I started designing this solution for the Arduino Uno Wifi but its software is not secure enough so I changed to the esp-link software stack from JeeLabs.

This is what this software is based on :
- Arduino Mega running this app,
- using the ELClient library (https://github.com/jeelabs/el-client),
- ESP8266 (nodemcu board),
- running JeeLabs's esp-link (https://github.com/jeelabs/esp-link)

Motor steering is performed via the (old style) Adafruit Motor Shield, which is widely
available from cheap manufacturers. Converting to the new shield should be simple.

The code can periodically feed environmental data to ThingSpeak, and to MQTT.

A significant issue is memory - current implementation already takes 75% both in program and
variable memory. Note that most of the variable space is in use by the ESP driver for the
Uno Wifi (more than half of the available memory !).

Timezone management is done in a simplistic way : the RTC needs to be in local time.
The time set code adds timezone offset, but nothing else manages timezones.

Note : I'm not using the Arduino IDE, I've supplied a Makefile. Using the sources
with the Arduino IDE should be possible too, it's been tested several times during
the development.
