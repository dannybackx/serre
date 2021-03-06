/*
 * Measurement station, with web server : ina3221 driver
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
#include "ina3221.h"
#include "measure.h"
#include <Control.h>

static SDL_Arduino_INA3221 *ina3221 = 0;
static time_t prev_ts = 0;

static int sensor = -1;

void ina3221_register(time_t ts, float bus_voltage, float shunt_voltage, float current) {
  control->RegisterData(sensor, ts);
  control->RegisterData(sensor, 0, bus_voltage);
  control->RegisterData(sensor, 1, shunt_voltage);
  control->RegisterData(sensor, 2, current);
}

void ina3221_begin() {
  // Register the sensor first, so we'll report about it whether or not it is present
  sensor = control->RegisterSensor("INA3221");
  control->SensorRegisterField(sensor, "bus_voltage", FT_FLOAT);
  control->SensorRegisterField(sensor, "shunt_voltage", FT_FLOAT);
  control->SensorRegisterField(sensor, "current", FT_FLOAT);

  ina3221 = new SDL_Arduino_INA3221();
  ina3221->begin();
  int manuf = ina3221->getManufID();
  // Serial.printf("INA3221 manufacturer : %04x\n", manuf);
  if (manuf == 0xFFFF) {	// Manufacturer id is 0xFFFF if sensor not connected
    Serial.println("No ina3321 sensor");
    delete ina3221;
    ina3221 = 0;
    return;
  }

  prev_ts = time(0);
}

void ina3221_loop(time_t now) {
  if ((now - prev_ts < 2) || (now < 1000))
    return;
  prev_ts = now;

  if (ina3221) {
    float	current, bus_voltage, shunt_voltage;

    bus_voltage = ina3221->getBusVoltage_V(1);
    shunt_voltage = ina3221->getShuntVoltage_mV(1);
    current = ina3221->getCurrent_mA(1);

    Serial.printf("Bus v %3.1f shunt v %3.1f current %3.1f (ts %s)\n", bus_voltage, shunt_voltage, current, timestamp(now));

    ina3221_register(now, bus_voltage, shunt_voltage, current);
  }
}
