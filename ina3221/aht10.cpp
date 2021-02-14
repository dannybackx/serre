/*
 * Measurement station, with web server
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
#include <Adafruit_AHTX0.h>
#include "aht10.h"
#include "measure.h"

Adafruit_AHTX0 aht;
bool sensor = false;
time_t prev_ts = 0;

struct aht_reg aht_reg[100];
int ix = 0;

void aht_register(time_t ts, float temp, float hum) {
  aht_reg[ix].ts = ts;
  aht_reg[ix].hum = hum;
  aht_reg[ix].temp = temp;
  ix = (ix + 1) % 100;
}

void aht10_begin() {
  if (! aht.begin())
    Serial.println("No AHT sensor");
  else
    sensor = true;

  prev_ts = time(0);
}

void aht10_loop(time_t now) {
  if ((now - prev_ts < 2) || (now < 1000))
    return;
  prev_ts = now;

  if (sensor) {
    sensors_event_t	humidity, temp;
    aht.getEvent(&humidity, &temp);
    Serial.printf("Temp %3.1f hum %2.0f (ts %s)\n", temp.temperature, humidity.relative_humidity, timestamp(now));

    aht_register(now, temp.temperature, humidity.relative_humidity);
  }
}
