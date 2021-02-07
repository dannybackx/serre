# Measurement station for I²C based sensors (especially the INA3221)
(c) 2021 by Danny Backx

This is a simple esp8266 based application to control a power switch.

The secrets.h file is only provided as a sample, you need to supply your own values.

You can remotely set or disable the relay as such :
 - to enable
```
    % mosquitto_pub -t /switch -m on
```
 - to disable
```
    % mosquitto_pub -t /switch -m off
```
 - to query the ESP's network settings
```
    % mosquitto_pub -t /switch -m network
```
 - to set the time schedule
```
    % mosquitto_pub -t /switch/program -m 06:45,1,07:35,0,21:00,1,22:35,0
```
 - to query the time schedule
```
    % mosquitto_pub -t /switch -m query
```

So obviously it responds to MQTT.

You can specify more than one network, if you have more than one Wifi network available in your home.

It's possible to update the app in the esp over OTA (type "make ota").

