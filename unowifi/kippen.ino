/*
 
 RestServer.ino
 
 Arduino Uno WiFi Ciao example
 
 This example for the Arduino Uno WiFi shows how to use the
 Ciao library to access the digital and analog pins
 on the board through REST calls. It demonstrates how
 you can create your own API when using REST style
 calls through the browser. 
 
 Possible commands created in this shetch:
 
 * "/arduino/digital/13"     -> digitalRead(13)
 * "/arduino/digital/13/1"   -> digitalWrite(13, HIGH)
 * "/arduino/analog/2/123"   -> analogWrite(2, 123)
 * "/arduino/analog/2"       -> analogRead(2)
 * "/arduino/mode/13/input"  -> pinMode(13, INPUT)
 * "/arduino/mode/13/output" -> pinMode(13, OUTPUT)
 
 This example code is part of the public domain
 
 http://www.arduino.org/learning/tutorials/restserver-and-restclient
 
*/
 
#include <Wire.h>
#include <ArduinoWiFi.h>
#include "buildinfo.h"
#include "SFE_BMP180.h"
#include "global.h"
 
SFE_BMP180	*bmp = 0;
double		newPressure, newTemperature, oldPressure, oldTemperature;
 
void setup() {
  Serial.begin(9600);
  Serial.println("Starting WiFi ...");
  Wifi.begin();

  Wifi.println("REST Server is up");

  Serial.print("REST server is up, build ");
  Serial.print(_BuildInfo.src_filename);
  Serial.print(" ");
  Serial.print(_BuildInfo.date);
  Serial.print(" ");
  Serial.print(_BuildInfo.time);
  Serial.println("");

  // BMP180 temperature and air pressure sensor
  Serial.print("Initializing BMP180 ... ");
  BMPInitialize();
  Serial.println(bmp ? "ok" : "failed");
}
 
void loop() {
 
    while(Wifi.available()){
      ProcessCallback(Wifi);
    }
  delay(50);
 
}
 
void BMPInitialize() {
  bmp = new SFE_BMP180();
  char ok = bmp->begin();	// 0 return value is a problem
  if (ok == 0) {
    delete bmp;
    bmp = 0;
  }
}

/*
 * Returns 0 on success, negative values on error
 * -1 not initialized
 * -2xx nan
 * -3 communication error
 */
int BMPQuery() {
  bool nan = false;

  if (bmp == 0)
    return -1;

  // This is a multi-part query to the I2C device, see the SFE_BMP180 source files.
  char d1 = bmp->startTemperature();
  delay(d1);
  char d2 = bmp->getTemperature(newTemperature);
  if (d2 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (temperature)\n");
#endif
    return -201;
  }

  char d3 = bmp->startPressure(0);
  delay(d3);
  char d4 = bmp->getPressure(newPressure, newTemperature);
  if (d4 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (pressure)\n");
#endif
    return -202;
  }

  if (isnan(newTemperature) || isinf(newTemperature)) {
    newTemperature = oldTemperature;
    nan = true;
  }
  if (isnan(newPressure) || isinf(newPressure)) {
    newPressure = oldPressure;
    nan = true;
  }
  if (nan) {
#ifdef DEBUG
    DEBUG.println("BMP180 nan");
#endif
    return -203;
  }
  return 0;
}
