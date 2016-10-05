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
 *
 * This contains code to send a message to ThingSpeak
 *
 * Inspiration for this code is at
 * https://github.com/tve/espduino/blob/master/espduino/examples/demo/demo.ino
 */
#include <Arduino.h>
#include <ArduinoWiFi.h>
#include "SFE_BMP180.h"
#include "Hatch.h"
#include "TimeLib.h"
#include "ThingSpeak.h"
#include "global.h"

extern ESP esp;

ThingSpeak::ThingSpeak() {
  lasttime = -1;
  delta = 600;				// FIXME 10 minutes
  rest = new REST(&esp);
  if (! rest->begin(gpm(ts_url))) {
    delete rest;
    rest = 0;
  }
}

ThingSpeak::~ThingSpeak() {
  delete rest;
  rest = 0;
}

/*
 * Report environmental information periodically
 */
void ThingSpeak::loop(time_t nowts) {
  if (lasttime < 0 || (nowts - lasttime > delta)) {
      lasttime = nowts;

      Serial.print("ThingSpeak feed at "); Serial.print(hour(nowts)+personal_timezone); Serial.print(':'); Serial.println(minute(nowts)); 
      if (bmp) {
        BMPQuery();

        int a, b, c;
        // Temperature
        a = (int) newTemperature;
        double td = newTemperature - a;
        b = 100 * td;
        c = newPressure;

        // Format the result
        sprintf(buffer, "/update?api_key=%s&field1=%d.%02d&field2=%d", gpm(ts_write_key), a, b, c);
	rest->get(buffer);
        int err = rest->getResponse(buffer, buffer_size);
	if (err == HTTP_STATUS_OK) {
	  // Serial.print("TS success "); Serial.println(buffer);
	} else if (err == 0) {
	  // timeout
	  Serial.print("TS timeout "); Serial.println(buffer);
	} else {
	  Serial.print("TS GET fail "); Serial.println(err);
	}
      }
    
  }
}

/*
 * Report motor stop/start
 */
void ThingSpeak::changeState(int state) {
      Serial.print("ThingSpeak state change to "); Serial.println(state);
      sprintf(buffer, "/update?api_key=%s&field3=%d", gpm(ts_write_key), state);
      rest->get(buffer);
      int err = rest->getResponse(buffer, buffer_size);
      if (err == HTTP_STATUS_OK) {
	// Serial.print("TS success "); Serial.println(buffer);
      } else if (err == 0) {
	// timeout
	Serial.print("TS timeout "); Serial.println(buffer);
      } else {
	Serial.print("TS GET fail "); Serial.println(err);
      }
}
