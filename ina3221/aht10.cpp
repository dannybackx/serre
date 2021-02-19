/*
 * Measurement station, with web server : driver for AHT10 sensor
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
#include <Control.h>

static Adafruit_AHTX0 *aht = 0;
static time_t prev_ts = 0;

static int sensor = 0;

void aht_register(time_t ts, float temp, float hum) {
  control->RegisterData(sensor, ts);
  control->RegisterData(sensor, 0, temp);
  control->RegisterData(sensor, 1, hum);
}

void aht10_begin() {
  // Register the sensor first, so we'll report about it whether or not it is present
  sensor = control->RegisterSensor("AHT10");
  control->SensorRegisterField(sensor, "temperature", FT_FLOAT);
  control->SensorRegisterField(sensor, "humidity", FT_FLOAT);

  aht = new Adafruit_AHTX0();
  if (! aht->begin()) {
    Serial.println("No AHT sensor");
    delete aht;
    aht = 0;
  }

  prev_ts = time(0);
}

void aht10_loop(time_t now) {
  if ((now - prev_ts < 2) || (now < 1000))
    return;
  prev_ts = now;

  if (aht != 0) {
    sensors_event_t	humidity, temp;
    aht->getEvent(&humidity, &temp);
    Serial.printf("Temp %3.1f hum %2.0f (ts %s)\n", temp.temperature, humidity.relative_humidity, timestamp(now));

    aht_register(now, temp.temperature, humidity.relative_humidity);
  }
}
