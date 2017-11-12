/*
 * Copyright (c) 2016, 2017 Danny Backx
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

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientRest.h>
#include <ELClientMqtt.h>

#include "SFE_BMP180.h"
#include "TimeLib.h"
#include "Light.h"
#include "Hatch.h"
#include "ThingSpeak.h"
#include "Sunset.h"
#include "global.h"

extern ELClient esp;

ThingSpeak::ThingSpeak(int test) {
  lasttime = -1;
  delta = 600;				// FIXME 10 minutes
  rest = new ELClientRest(&esp);
  int err = rest->begin(ts_url);
  if (err != 0) {
    delete rest;
    rest = 0;

    Serial.print("ThingSpeak : could not initialize ");
    Serial.print(ts_url);
    Serial.print(" , error code ");
    Serial.println(err);
  }

  if (test)
    write_key = test_ts_write_key;
  else
    write_key = ts_write_key;
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
    char sb[96];
    lasttime = nowts;

    if (bmp == 0)
      BMPInitialize();

    if (bmp) {
      BMPQuery();

      int a, b, c;
      // Temperature
      a = (int) newTemperature;
      double td = newTemperature - a;
      b = 100 * td;
      c = newPressure;

      int l = light->query();

      if (rest != 0) {
	// Format the result
	sprintf(sb, "/update?api_key=%s&field1=%d.%02d&field2=%d&field4=%d",
	  write_key, a, b, c, l);

	// Send to ThingSpeak
	rest->get(sb);

	// This almost certainly gets a timeout because we didn't wait
        err = rest->getResponse(sb, 96);
	if (err == HTTP_STATUS_OK) {
	  // Serial.print("TS success "); Serial.println(buffer);
	} else if (err == 0) {
	  // timeout, but it's a phony one so ignore it !
	  // Serial.print("ThingSpeak timeout "); Serial.println(sb);
	} else {
	  Serial.print("ThingSpeak GET failure "); Serial.println(err);
	}
      }

      sprintf(sb, "%02d:%02d:%02d %02d/%02d/%04d %d.%02d,%d,%d",
	hour(), minute(), second(), day(), month(), year(),
	a, b, c, l);
      mqttSend(sb);
    } else {
      // No BMP but dial home anyway
      sprintf(sb, "kippen, no BMP sensor");
      mqttSend(sb);
    }
  }
}

/*
 * Report motor motion and hatch state
 */
void ThingSpeak::changeState(int hr, int mn, int sec, int motion, int state, char *msg) {
  // Serial.print("ThingSpeak state change to "); Serial.println(state);

  sprintf(buffer, "/update?api_key=%s&field3=%d&field5=%d" , write_key, motion, state);
  rest->get(buffer);

  int err = rest->getResponse(buffer, buffer_size);
  if (err == HTTP_STATUS_OK) {
    // Serial.print("TS success "); Serial.println(buffer);
  } else if (err == 0) {
    // timeout
    Serial.print("ThingSpeak timeout "); Serial.println(buffer);
  } else {
    Serial.print("ThingSpeak GET failure "); Serial.println(err);
  }

  sprintf(buffer, "Time %02d:%02d:%02d motion %d, state %d (%s)",
    hr, mn, sec, motion, state, (msg == NULL) ? "" : msg);
  mqttSend(buffer);
}
