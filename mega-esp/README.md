#
# Chicken House Hatch and Sensor Module
#
# esp-link + Arduino Mega 2560
#
# (c) 2016, 2017 by Danny Backx
#

This module is meant to power a hatch that protects your chickens against nightly predators
by closing the hatch at night.

The hatch design is assumed to have a magnet and two sensors, so the controller can feel
when to stop the motor. It gets its time from a RTC (real time clock) module or over Internet,
and can feed environmental data if equipped with a sensor module (BMP180).

I started designing this solution for the Arduino Uno Wifi but its software is not secure enough so I changed to the esp-link software stack from JeeLabs.

This is what this software is based on :
- Arduino Mega running this app,
- using the ELClient library (https://github.com/jeelabs/el-client),
- ESP8266 (nodemcu board),
- running JeeLabs's esp-link (https://github.com/jeelabs/esp-link)

- a simple L298N motor steering module (older code uses the old style Adafruit Motor Shield)
- a LDR (light dependent resistor) to detect day/night
- a BMP180 sensor to monitor temperature and air pressure
- two analog hall effect sensors (SS49E for instance) to detect door open/close

The code can periodically feed environmental data to ThingSpeak, and to MQTT.

There's no way to run this on an Arduino Uno : it simply doesn't have enough memory.

The default configuration is to use time information from esp-link. If you use my
proposed addition (to esp-link), DST is automatically handled. If you use a real RTC
then you need to put the clock in local time, and handle DST yourself.

I don't use the Arduino IDE, I've supplied a Makefile. Using the sources
with the Arduino IDE should be possible too, it's been tested several times during
the development.

Some of the data required to make this work are private. The real secrets.c file that
I use is not made available publically. Take the secrets.c.sample and adapt it for your
environment (your wifi network name and password, etc.).

The personal.c file contains settings specific to my environment. You'll want to review
and personalize those too.
