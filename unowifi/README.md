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

My prototype runs on a Arduino Uno WiFi, a board which combines an ATmega328 (as the main
processor) and an ESP8266 (although more powerful, only used for WiFi). More info is
available at http://www.arduino.org/products/boards/arduino-uno-wifi .

Motor steering is performed via the (old style) Adafruit Motor Shield, which is widely
available from cheap manufacturers. Converting to the new shield should be simple.

The code can periodically feed environmental data to ThingSpeak.
This is work in progress.

A significant issue is memory - current implementation already takes 75% both in program and
variable memory. Note that most of the variable space is in use by the ESP driver for the
Uno Wifi (more than half of the available memory !).

A minor issue is timezone management. That's done in a simplistic way, by adding a timezone
offset (in hours) to the hours returned by the RTC. As I live in a place where this
is not a large number, this doesn't conflict with anything. (It offsets possible values
of hr from 0..23 to 2..25 in my case. There's no problem as long as your scheduled
hours don't conflict with that, morning schedules for Western US might be at risk.)


Note : I'm not using the Arduino IDE, I've supplied a Makefile. Using the sources
from the Arduino IDE should be possible too, it's been tested several times during
the development.
