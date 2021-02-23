/*
 * Measurement station, with web server : allow triggers on ESP8266 pins
 *
 * Copyright (c) 2021 Danny Backx
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
#include "measure.h"
#include <Control.h>

static time_t prev_ts = 0;

static int sensor = 0;
static uint8 p5, p6, p7;

void d1mini_begin() {
  // Register the sensor first, so we'll report about it whether or not it is present
  sensor = control->RegisterSensor("d1mini");
  control->SensorRegisterField(sensor, "d5", FT_PIN);
  control->SensorRegisterField(sensor, "d6", FT_PIN);
  control->SensorRegisterField(sensor, "d7", FT_PIN);

  prev_ts = time(0);

  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  pinMode(D7, INPUT);
}

void d1mini_loop(time_t now) {
  int delta = control->measureDelay(sensor, now);

  if ((now - prev_ts < delta) || (now < 1000))
    return;
  prev_ts = now;

  p5 = digitalRead(D5);
  p6 = digitalRead(D6);
  p7 = digitalRead(D7);

  if (control->isRegistering(sensor, now, p5, p6, p7)) {
    // Never actually log something
  }
}
