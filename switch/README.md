# Remote / programmable power switch
(c) 2017 by Danny Backx

This is a simple esp8266 based application to control a power switch.

Hook up a cheap ESP8266 processor, a small power supply, and a relay (SSR or real); and you're in business.
It's also possible to replace the internals of a power plug, as I did with my (not so cheap) Belkin Wemos Switch after I found it became unsupported and unreliable.

The secrets.h file is only provided as a sample, you need to supply your own values.

You can remotely set or disable the relay as such :
 - to enable
```
    % mosquitto_pub -t /switch/on -m x
```
 - to disable
```
    % mosquitto_pub -t /switch/off -m x
```
 - to query the ESP's network settings
```
    % mosquitto_pub -t /switch/network -m x
```
 - to set the time schedule
```
    % mosquitto_pub -t /switch/program -m 06:45,1,07:35,0,21:00,1,22:35,0
```
 - to query the time schedule
```
    % mosquitto_pub -t /switch/query -m x
```

So obviously it responds to MQTT.

You can specify more than one network, if you have more than one Wifi network available in your home.

The schedule provided in the example above is the default. The switch will provide power between 6:45am and 7:35am,
and between 9pm and 10:35pm. The schedule takes DST into account (after a restart).

It's possible to update the app in the esp over OTA (type "make ota").

The app console should show something like this :
```

Power switch
(c) 2017 by Danny Backx

Boot version 6, flash chip size 4194304, SDK version 1.5.2
Free sketch space 786432, application build 2017-08-25 16:33:15
Starting WiFi . MAC XX:XX:XX:XX:XX:XX, SSID {XXXXXXXXX}, IP 192.168.1.XXX, GW 192.168.1.XXX
Initialize SNTP ...
Starting OTA listener ...
Time .....  2017-08-25 16:33:41
Ready!
Attempting MQTT connection...connected

```
