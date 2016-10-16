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
#include "SFE_BMP180.h"
#include <TimeLib.h>
#include "Hatch.h"
#include "Light.h"
#include "ThingSpeak.h"
#include <DS1307RTC.h>
#include "Ifttt.h"
#include "global.h"

#ifdef BUILT_BY_MAKE
#include "buildinfo.h"
#endif

time_t		boot_time = 0;

SFE_BMP180	*bmp = 0;
double		newPressure, newTemperature, oldPressure, oldTemperature;
char		buffer[buffer_size];

Hatch		*hatch = 0;
Light		*light = 0;
int		newhatch, oldhatch;
enum lightState	newlight, oldlight;
int		sensor_up, sensor_down, button_up, button_down;

ThingSpeak	*ts = 0;

void setup() {
  delay(2000);
  Serial.begin(9600);

  // Display startup text
  Serial.println(gpm(startup_text1));
  Serial.println(gpm(startup_text2));

  // Start WiFi
  Serial.println(gpm(starting_wifi));
  Wifi.begin();

  // If built by make, talk about specifics
#ifdef BUILT_BY_MAKE
#warning "This takes a lot of memory, especially due to the file name"
  Serial.print(gpm(server_build));
  Serial.print(_BuildInfo.src_filename);
  Serial.print(" ");
  Serial.print(_BuildInfo.date);
  Serial.print(" ");
  Serial.println(_BuildInfo.time);
#endif

  // BMP180 temperature and air pressure sensor
  Serial.print(gpm(initializing_bmp180));
  BMPInitialize();
  Serial.println(bmp ? "ok" : "failed");

  // Time
  setSyncProvider(RTC.get);	// Set the function to get time from RTC
  if (timeStatus() != timeSet)
    Serial.println(gpm(rtc_failure));
  else {
    Serial.print("RTC ok, ");      
    boot_time = now();
    sprintf(buffer, gpm(timedate_fmt),
      hour(), minute(), second(), day(), month(), year());
    Serial.println(buffer); 
  }

  // Hatch
  Serial.println(F("Set up hatch ..."));
  hatch = new Hatch(schedule_string);
  hatch->setMotor(3);
  hatch->setMaxTime(6);

  newhatch = oldhatch = 0;

  // Sensors and buttons
  ActivatePin(sensor_up_pin, gpm(sensor_up_string));
  ActivatePin(sensor_down_pin, gpm(sensor_down_string));
  ActivatePin(button_up_pin, gpm(button_up_string));
  ActivatePin(button_up_pin, gpm(button_down_string));

  light = new Light();
  light->setSensorPin(light_sensor_pin);
  ActivatePin(light_sensor_pin, gpm(light_sensor_string));

  // Initialize sensor states
  sensor_up = sensor_down = button_up = button_down = -1;
  if (sensor_up_pin >= 0)  {
    sensor_up = digitalRead(sensor_up_pin);

    if (sensor_up == 1)
      hatch->IsUp();
  }
  if (sensor_down_pin >= 0) {
    sensor_down = digitalRead(sensor_down_pin);

    if (sensor_down == 1)
      hatch->IsDown();
  }
  if (button_up_pin >= 0)
    button_up = digitalRead(button_up_pin);
  if (button_down_pin >= 0)
    button_down = digitalRead(button_down_pin);

  ts = new ThingSpeak();

#ifdef USE_IFTTT
  Ifttt *ifttt = new Ifttt();
  ifttt->sendEvent(gpm(ifttt_key), (char *)ifttt_event, "Boot");
#endif

  // Yeah !
  Serial.println(gpm(ready));
  mqtt(gpm(ready));
}
 
void ActivatePin(int pin, const char *name) {
  if (pin >= 0) {
    Serial.print(name);
    Serial.print(F(" is on pin "));
    Serial.println(pin);
    pinMode(sensor_up_pin, INPUT);
  } else {
    Serial.print(F("No "));
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

  time_t nowts = now();
  
  oldlight = newlight;
  newlight = light->loop(nowts);
  if (oldlight != newlight) {
    if (newlight == LIGHT_MORNING)
      hatch->Up(hour(nowts), minute(nowts), second(nowts));
    else if (newlight == LIGHT_EVENING)
      hatch->Down(hour(nowts), minute(nowts), second(nowts));
  }

  // Note the hatch->loop code will also trigger the motor
  if (hatch) {
    oldhatch = newhatch;
    newhatch = hatch->loop(hour(nowts), minute(nowts), second(nowts));
  }

  // Sensors stop motion
  if (sensor_up_pin >= 0) {
    int oldvalue = sensor_up;
    sensor_up = digitalRead(sensor_up_pin);
    if (oldvalue != sensor_up && sensor_up == 1) {
      // Stop moving the hatch
      hatch->Stop();
      hatch->IsUp();
    }
  }
  if (sensor_down_pin >= 0) {
    int oldvalue = sensor_up;
    sensor_down = digitalRead(sensor_down_pin);
    if (oldvalue != sensor_down && sensor_down == 1) {
      // Stop moving the hatch
      hatch->Stop();
      hatch->IsDown();
    }
  }

  // Buttons start motion
  if (button_up_pin >= 0) {
    int oldvalue = button_up;
    button_up = digitalRead(button_up_pin);
    if (oldvalue != button_up && button_up == 1) {
      // Move the hatch up
      hatch->Up();
    }
  }
  if (button_down_pin >= 0) {
    int oldvalue = button_up;
    button_down = digitalRead(button_down_pin);
    if (oldvalue != button_down && button_down == 1) {
      // Move the hatch down
      hatch->Down();
    }
  }

  ts->loop(nowts);

  delay(loop_delay);
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
    DEBUG.printf(F("BMP180 : communication error (pressure)\n"));
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
    DEBUG.println(F("BMP180 nan"));
#endif
    return -203;
  }
  return 0;
}

#include "mqtt.h"

extern ESP esp;
MQTT _mqtt(&esp);

void mqtt(const char *msg) {
  _mqtt.publish(mqtt_topic, (char *)msg);
}
