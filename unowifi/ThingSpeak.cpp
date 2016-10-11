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
#include "TimeLib.h"
#include "Light.h"
#include "Hatch.h"
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
  int err;

  if (lasttime < 0 || (nowts - lasttime > delta)) {
      lasttime = nowts;

      Serial.print(gpm(ts_feed)); Serial.print(hour(nowts)+personal_timezone); Serial.print(':'); Serial.println(minute(nowts)); 
      if (bmp) {
        BMPQuery();

        int a, b, c;
        // Temperature
        a = (int) newTemperature;
        double td = newTemperature - a;
        b = 100 * td;
        c = newPressure;

	int l = light->query();

	char *sb = (char *)malloc(96);
        if (sb != NULL) {
	  // Format the result
          sprintf(sb, gpm(ts_123), ts_write_key, a, b, c, l);
	  Serial.print(gpm(ts_colon)); Serial.println(sb);

	  rest->get(sb);
          err = rest->getResponse(sb, 96);
	  if (err == HTTP_STATUS_OK) {
	    // Serial.print("TS success "); Serial.println(buffer);
	  } else if (err == 0) {
	    // timeout
	    Serial.print(gpm(ts_timeout)); Serial.println(sb);
	  } else {
	    Serial.print(gpm(ts_get_fail)); Serial.println(err);
	  }
	  free(sb);
	} else {
	  Serial.println(gpm(out_of_memory));
	}
      }
    
  }
}

/*
 * Report motor stop/start
 */
void ThingSpeak::changeState(int state) {
      Serial.print(gpm(ts_state_change)); Serial.println(state);
      sprintf(buffer, gpm(ts_4), ts_write_key, state);
      rest->get(buffer);
      int err = rest->getResponse(buffer, buffer_size);
      if (err == HTTP_STATUS_OK) {
	// Serial.print("TS success "); Serial.println(buffer);
      } else if (err == 0) {
	// timeout
	Serial.print(gpm(ts_timeout)); Serial.println(buffer);
      } else {
	Serial.print(gpm(ts_get_fail)); Serial.println(err);
      }
}
