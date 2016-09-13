#
# MQTT Chicken House Hatch and Sensor Module
#
# (c) 2016 by Danny Backx
#

This module is meant to power a hatch that protects your chickens against nightly predators
by closing the hatch at night.

The hatch design is assumed to have a magnet and two sensors, so the controller can feel
when to stop the motor.

My prototype runs on a Wemos D1 (one of the old boards, the R1 version).
This is a Arduino compatible board powered by an ESP8266.

Motor steering is performed via the (again, old style) Adafruit Motor Shield, which is widely
available from cheap manufacturers. Converting to the new shield should be simple.

MQTT can be used to alter settings, query the module, or get periodic reports. Example :
  host% mosquitto_pub -h pi -t Serre -m Serre/System/Info

  generates this output
  --> System Info Boot version 31, flash chip size 4194304, SDK version 1.5.2(7eee54f4)
  --> System Info MAC 60:01:94:05:7C:8B SSID {WiFi-2.4-44DF}, IP 192.168.1.101, GW 192.168.1.1

  in a window with command
  host% mosquitto_sub -h pi -d -t Kippen -t Kippen/+ -t Kippen/+/+

  In these examples, "pi" is the host running mosquitto, the MQTT broker I use.

The WiFi startup code is written such that the parameters of your network can be
stored in the single file mywifi.h . A sample version of this file is provided,
I'm not sharing mine as it contains configuration settings of my network.

More customisation can be done by editing personal.c .

Sensor or event data can be sent to ThingSpeak by configuring a channel on that site,
and copying the channel id and the keys to mywifi.h . The time interval between reports
is configurable in personal.c (default 900 seconds).

It can be queried with the ThingSpeak APIs such as
  curl -o feed.json -X GET "https://api.thingspeak.com/channels/157972/feeds.json?api_key={channel_read_key}&results=500"

Documentation on that is at
  https://nl.mathworks.com/help/thingspeak/get-a-channel-feed.html
