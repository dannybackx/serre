/*
 * Copyright (c) 2016 Danny Backx
 *
 * License (MIT license):
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 */
#include <Wire.h>
#include <ArduinoWiFi.h>
#include "buildinfo.h"
#include "SFE_BMP180.h"
#include "global.h"
#include "Hatch.h"

#include <TimeLib.h>
#include <DS1307RTC.h>
 
SFE_BMP180	*bmp = 0;
double		newPressure, newTemperature, oldPressure, oldTemperature;
char		buffer[64];

Hatch		*hatch = 0;
int		state, oldstate;
int	sensor_up, sensor_down, button_up, button_down;
 
void setup() {
  Serial.begin(9600);
  Serial.println("Starting WiFi ...");
  Wifi.begin();

  Serial.println(startup_text);
  Wifi.println(startup_text);

  Serial.print("Server build ");
  Serial.print(_BuildInfo.src_filename);
  Serial.print(" ");
  Serial.print(_BuildInfo.date);
  Serial.print(" ");
  Serial.println(_BuildInfo.time);
#if 0
  // BMP180 temperature and air pressure sensor
  Serial.print("Initializing BMP180 ... ");
  BMPInitialize();
  Serial.println(bmp ? "ok" : "failed");
#endif
  // Time
  setSyncProvider(RTC.get);	// Set the function to get time from RTC
  if (timeStatus() != timeSet)
    Serial.println("Unable to sync with the RTC");
  else {
    Serial.print("RTC ok, ");      
    sprintf(buffer, "%02d:%02d:%02d %02d/%02d/%04d",
      hour(), minute(), second(), day(), month(), year());
    Serial.println(buffer); 
  }

  // Hatch
  Serial.println("Set up hatch ...");
  hatch = new Hatch(schedule_string);
  hatch->setMotor(3);

  state = oldstate = 0;

  ActivatePin(sensor_up_pin, "Sensor UP");
  ActivatePin(sensor_down_pin, "Sensor DOWN");
  ActivatePin(button_up_pin, "Button UP");
  ActivatePin(button_up_pin, "Button Down");

  sensor_up = sensor_down = button_up = button_down = 0;

  Serial.println("Ready");
}
 
void ActivatePin(int pin, char *name) {
  if (pin >= 0) {
    Serial.print(name);
    Serial.print(" is on pin ");
    Serial.println(pin);
    pinMode(sensor_up_pin, INPUT);
  } else {
    Serial.print("No ");
    Serial.println(name);
  }
}

/*********************************************************************************
 *                                                                               *
 * Loop                                                                          *
 *                                                                               *
 *********************************************************************************/
void loop() {
  while(Wifi.available()){
    ProcessCallback(Wifi);
  }

  // Note the hatch->loop code will also trigger the motor
  if (hatch) {
    oldstate = state;
    state = hatch->loop(hour(), minute());
  }

  // Sensors stop motion
  if (sensor_up_pin >= 0) {
    int oldvalue = sensor_up;
    sensor_up = digitalRead(sensor_up_pin);
    if (oldvalue != sensor_up && sensor_up == 1) {
      // Stop moving the hatch
      hatch->Stop();
    }
  }
  if (sensor_down_pin >= 0) {
    int oldvalue = sensor_up;
    sensor_down = digitalRead(sensor_down_pin);
    if (oldvalue != sensor_down && sensor_down == 1) {
      // Stop moving the hatch
      hatch->Stop();
    }
  }

  // Buttons start motion
  if (button_up_pin >= 0) {
    int oldvalue = button_up;
    button_up = digitalRead(button_up_pin);
    if (oldvalue != button_up && button_up == 1) {
      // Stop moving the hatch
      hatch->Up();
    }
  }
  if (button_down_pin >= 0) {
    int oldvalue = button_up;
    button_down = digitalRead(button_down_pin);
    if (oldvalue != button_down && button_down == 1) {
      // Stop moving the hatch
      hatch->Down();
    }
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

void ProcessCallback(WifiData client) {
  String command = client.readString();
 
  Serial.print("ProcessCallback {");
  Serial.print(command);
  Serial.println("}");

  if (command == mqtt_topic_bmp180) {
      if (bmp) {
        BMPQuery();

        int a, b, c;
        // Temperature
        a = (int) newTemperature;
        double td = newTemperature - a;
        b = 100 * td;
        c = newPressure;

        // Format the result
        sprintf(reply, "bmp (%2d.%02d, %d)", a, b, c);
  
      } else {
        sprintf(reply, "No sensor detected");
      }
    Wifi.print(reply);
  } else if (command.startsWith("/arduino/digital/time/set/")) {
  // Add cases here like
  // } else if (command.startsWith("/arduino/digital/time/set/")) {
  // } else if (command == "/arduino/digital/time/set/") {
  } else {
  }
  client.print(EOL);
}

