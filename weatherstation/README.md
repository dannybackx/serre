#Garden environment sensor

This measures temperature, humidity, pressure and uploads to Wunderground.
You'll have to copy the secrets.h.sample file into secrets.h and supply your own data.

This uses a ESP8266 and sensors like BMP280 or BME280.

Supports software updates via OTA, and multiple WiFi networks.

I borrowed the idea and a couple of lines of code from David Birds work.
The rest is (c) Danny Backx 2018.

Intention is to use stuff I have around :
- use a esp8266 (a Wemos D1 Mini)
- put in a small almost sealed box
- couple with an old Nokia 5V power supply, and a capacitor
- attach a BME280 sensor
and provide this functionality :
- feed Wunderground
- software update possible via OTA
- also MQTT support (logging to my server, remote query and reboot).

You can point to your MQTT server via a tunnel in your router, by using a service
such as no-ip.com to point to an external address. This allows you to use another
network than your home WiFi and still log data both on Wunderground and locally.

https://github.com/finitespace/BME280
