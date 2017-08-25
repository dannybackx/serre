# Remote / programmable power switch
(c) 2017 by Danny Backx

This is a simple esp8266 based application to control a power switch.

Hook up a cheap ESP8266 processor, a small power supply, and a relay (SSR or real); and you're in business.

The secrets.h file is only provided as a sample, you need to supply your own values.

You can remotely set or disable the relay as such :
 - to set
    % mosquitto_pub -t /switch/set -m x
 - to disable
    % mosquitto_pub -t /switch/reset -m x
 - to query the ESP's network settings
    % mosquitto_pub -t /switch/network -m x
 - to set the time schedule
    % mosquitto_pub -t /switch/program -m 06:45,1,07:35,0,21:00,1,22:35,0
 - to query the time schedule
    % mosquitto_pub -t /switch/query -m x

So obviously it responds to MQTT.

You can specify more than one network, if you have more than one Wifi network available in your home.

The schedule provided in the example above is the default. The switch will provide power between 6:45am and 7:35am,
and between 9pm and 10:35pm.
