#
# Chicken House Hatch and Sensor Module
# Arduino Uno WiFi version
#
# (c) 2016 by Danny Backx
#

This module is meant to power a hatch that protects your chickens against nightly predators
by closing the hatch at night.

The hatch design is assumed to have a magnet and two sensors, so the controller can feel
when to stop the motor.

My prototype runs on a Arduino Uno WiFi, a board which combines an ATmega328 (as the main
processor) and an ESP8266 (although more powerful, only used for WiFi). More info is
available at http://www.arduino.org/products/boards/arduino-uno-wifi .

Motor steering is performed via the (old style) Adafruit Motor Shield, which is widely
available from cheap manufacturers. Converting to the new shield should be simple.

This is work in progress.
I'm hoping to hook it up to ThingSpeak, SNTP time service, etc
