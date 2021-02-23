/*
 * Measurement station, with web server : driver for ADS1115 ADC
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
#include "ads1115.h"
#include "measure.h"
#include <Control.h>

static ADS1115_WE *ads = 0;
static time_t prev_ts = 0;

static int sensor = 0;

void ads1115_begin() {
  // Register the sensor first, so we'll report about it whether or not it is present
  sensor = control->RegisterSensor("ADS1115");
  control->SensorRegisterField(sensor, "a0", FT_FLOAT);
  control->SensorRegisterField(sensor, "a1", FT_FLOAT);
  control->SensorRegisterField(sensor, "a2", FT_FLOAT);
  // control->SensorRegisterField(sensor, "a3", FT_FLOAT);

  ads = new ADS1115_WE();
  if (! ads->init()) {
    Serial.println("No ADS1115 sensor");
    delete ads;
    ads = 0;
  }

  prev_ts = time(0);

  ads->setVoltageRange_mV(ADS1115_RANGE_6144);
  ads->setCompareChannels(ADS1115_COMP_0_GND);
  ads->setMeasureMode(ADS1115_CONTINUOUS);
}

void ads1115_loop(time_t now) {
  int delta = control->measureDelay(sensor, now);

  if ((ads == 0) || (now - prev_ts < delta) || (now < 1000))
    return;
  prev_ts = now;

  float mv = ads->getResult_mV();
  Serial.printf("Measure %3.1f mV (ts %s)\n", mv, timestamp(now));

  if (control->isRegistering(sensor, now, mv, 0.0, 0.0)) {
    control->RegisterData(sensor, now);
    control->RegisterData(sensor, 0, mv);
  }
}
