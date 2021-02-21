/*
 * Measurement station : control class
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

#include <Control.h>

Control *control;

Control::Control() {
  control = this;
  nalloc = 0;
  next = 0;
  nsensors = 0;
  
  for (int i=0; i<MAX_SENSORS; i++) {
    sensors[i].name = 0;
    sensors[i].fn = 0;
  }
}

Control::~Control() {
  control = 0;
  if (data) {
    free((void *)data);
    data = 0;
  }
}

bool Control::SetStartCondition() {
}

void Control::Start() {
}

void Control::Stop() {
}

bool Control::setAllocation(int num) {
  nalloc = num;
  return AllocateMemory();
}

int Control::getAllocation() {
  return nalloc;
}

void Control::LogData() {
}

int Control::RegisterSensor(const char *name) {
  // Serial.printf("RegisterSensor(%s) -> %d\n", name, nsensors);
  if (nsensors == MAX_SENSORS)
    return -1;
  sensors[nsensors].name = name;
  sensors[nsensors].fn = 0;
  return nsensors++;
}

void Control::SensorRegisterField(int sensor, char *field, ft field_type) {
  int fn = sensors[nsensors].fn++;

  sensors[sensor].fields[fn] = field;
  sensors[sensor].fieldtypes[fn] = field_type;
}

bool Control::AllocateMemory() {
  data = (sensordata *)malloc(nalloc * sizeof(sensordata));
  if (! data)
    return false;

  for (int i=0; i<nalloc; i++) {
    data[i].sensorid = -1;
    data[i].ts = 0;
  }
  return true;
}

void Control::RegisterData(int sensor, time_t ts) {
  next = (next + 1) % nalloc;
  data[next].ts = ts;
  data[next].sensorid = sensor;
  for (int i=0; i<MAX_FIELDS; i++)
    data[next].data[i].i = 0;
}

void Control::RegisterData(int sensor, int field, float value) {
  data[next].data[field].f = value;
}

void Control::RegisterData(int sensor, int field, int value) {
  data[next].data[field].i = value;
}

time_t Control::getTimestamp(int ix) {
  if (ix < 0 || ix >= nalloc || data == 0)
    return 0;
  return data[ix].ts;
}

ft Control::getFieldType(int sensor, int field) {
  if (sensor < 0 || sensor >= nsensors || field < 0 || field >= MAX_FIELDS)
    return FT_NONE;
  return sensors[sensor].fieldtypes[field];
}

const char *Control::getFieldName(int sensor, int field) {
  if (sensor < 0 || sensor >= nsensors || field < 0 || field >= MAX_FIELDS)
    return 0;
  return sensors[sensor].fields[field];
}

const char *Control::getSensorName(int sensor) {
  if (sensor < 0 || sensor >= nsensors) {
    return 0;
  }
  return sensors[sensor].name;
}

int Control::getDataSensor(int ix) {
  if (ix < 0 || ix >= nalloc)
    return -1;
  return data[ix].sensorid;
}

float Control::getDataFloat(int ix, int field) {
  return data[ix].data[field].f;
}

int Control::getDataInt(int ix, int field) {
  return data[ix].data[field].i;
}
