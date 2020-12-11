Small program to drive a couple of RGB LED strips for the winter period.
Currently contains basic code to talk to wifi, mqtt, and allow OTA.

Copyright (c) 2020 Danny Backx

First piece of code is just FastLED's XYMatrix example, to be extended.

To make this work, rename the secrets.h.sample file (remove the .sample) and insert
your own WiFi credentials.


An example LED strip : https://www.aliexpress.com/item/1m-4m-5m-WS2812B-Smart-led-pixel-strip-Black-White-PCB-30-60-144-leds-m/2036819167.html?spm=a2g0s.12269583.0.0.68f93443iStNVr 
has characteristics :
	60 LEDs per meter (12 mm between LEDs, a LED every 16.6mm)
	each LED (set of LEDs to form an RGB spot) driven by a WS2814B
	SMD5050
	5V, est 0.3W per LED

So an assembly with 2m of LED strip (120 LEDs) consumes 0.3 * 120 = 36W.
The 5V power supply must be able to deliver 36 / 5 = 7A for all LEDs to burn simultaneously.

Example 1 :
	Square, 20 cm across -> strips of 20cm = 12 LEDs (actually 13 : one at offset 0)
	13 such strips to form the square, so 169 LEDs, consumes 50.7W if all are on.

Example 2 :
	20 cm x 30 cm rectangle
	13 strips, of 19 LEDs
	Power consumption if all on : 74W
	This is about 4 meters worth of LED strip

Example 3 :
	Christmas star, 5 x 20 cm, or 60 LEDs in total.
	Power : 18W
