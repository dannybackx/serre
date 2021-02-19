# Measurement station for I²C based sensors (especially the INA3221)
(c) 2021 by Danny Backx

This is a simple esp8266 based application to measure stuff.
Goal is to use a sensor like a ina3221 to measure electrical parameters of a device under test,
and to be able to record the measurements,
and tune the measurement frequency interactively.

The secrets.h file is only provided as a sample, you need to supply your own values.
You can specify more than one network, if you have more than one Wifi network available in your home.

It's possible to update the app in the esp over OTA (type "make ota").

We have a web interface to control the application.
Parameters can be set on when and how (long, frequent) to measure. This includes start and stop conditions.

Data query is possible via web interface or JSON.
