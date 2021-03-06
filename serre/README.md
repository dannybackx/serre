#
# MQTT Greenhouse Watering and Sensor Module
#
# (c) 2016 by Danny Backx
#

This module is meant to power a water pump or valve

MQTT can be used to alter settings, query the module, or get periodic reports.

My prototype runs on a Wemos D1 (one of the old boards, the R1 version).
This is a Arduino compatible board powered by an ESP8266.

The WiFi startup code is written such that the parameters of your network can be
stored in the single file mywifi.h . A sample version of this file is provided,
I'm not sharing mine as it contains configuration settings of my network.

More customisation can be done by editing personal.c .

Sensor or event data can be sent to ThingSpeak by configuring a channel on that site,
and copying the channel id and the keys to mywifi.h . The time interval between reports
is configurable in personal.c (default 900 seconds).

It can be queried with the ThingSpeak APIs such as
  curl -o feed.json -X GET "https://api.thingspeak.com/channels/{channel}/feeds.json?api_key={channel_read_key}&results=500"

Documentation on that is at
  https://nl.mathworks.com/help/thingspeak/get-a-channel-feed.html

The MQTT server can be specified as an IP address, or as a fully qualified domain name.
For the latter, it is possible to use a dynamic DNS address, but obviously you'll need
to do the right port mapping so your MQTT broker is reachable.

So your MQTT server address might be specified via
  #define MQTT_EXT_SERVER "mqtt.MeMyselfAndI.dyndns.org"
  #define MQTT_EXT_PORT   1883

The assumption is that you use an IP address for a server on your internal network, and
a FQDN if you're on an public network (external to your home network).
Example :
  #define MQTT_SERVER "192.168.1.100"
