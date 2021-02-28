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
    sensors[i].mdelay = 10;
    sensors[i].ts = 0;
    for (int j=0; j<MAX_FIELDS; j++)
      sensors[i].data[j].i = 0;
  }
  for (int i=0; i<MAX_TRIGGERS; i++) {
    triggers[i].trigger_min = false;
    triggers[i].trigger_max = false;
  }
}

Control::~Control() {
  control = 0;
  if (data) {
    free((void *)data);
    data = 0;
  }
}

// Return success indicator
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

// Return success indicator
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

void Control::sensorData(uint8 sid, time_t ts, float a, float b, float c, float d) {
  sensors[sid].ts = ts;
  sensors[sid].data[0].f = a;
  sensors[sid].data[1].f = b;
  sensors[sid].data[2].f = c;
  sensors[sid].data[3].f = d;
}

void Control::sensorData(uint8 sid, time_t ts, uint32 a, uint32 b, uint32 c, uint32 d) {
  sensors[sid].ts = ts;
  sensors[sid].data[0].i = a;
  sensors[sid].data[1].i = b;
  sensors[sid].data[2].i = c;
  sensors[sid].data[3].i = d;
}

/*
 * This function serves two purposes : to collect info on which we possibly judge whether to log info,
 * and to return the answer whether we log info.
 *
 * Depending on settings, the output may not be based on the input. (E.g. if the user pushed the start button.)
 */
bool Control::isRegistering(uint8 sid, time_t ts, float a, float b, float c, float d) {
  sensorData(sid, ts, a, b, c, d);
  return isRegistering(sid);
}

bool Control::isRegistering(uint8 sid, time_t ts, uint32 a, uint32 b, uint32 c, uint32 d) {
  sensorData(sid, ts, a, b, c, d);
  return isRegistering(sid);
}

bool Control::isRegistering(uint8 sid) {
  // Global or per sensor override
  if (manual_stop)
    return false;
  if (manual_start)
    return true;

  for (int i=0; i<MAX_TRIGGERS; i++) {
    if (triggers[i].trigger_min) {
      uint8_t sid = triggers[i].sensorid;
      uint8_t field = triggers[i].field;
      enum ft tp = sensors[sid].fieldtypes[field];
      if (tp == FT_FLOAT) {
        if (sensors[sid].data[field].f >= triggers[i].data_min.f)
	  return true;
      } else if (tp == FT_INT) {
        if (sensors[sid].data[field].i >= triggers[i].data_min.i)
	  return true;
      }
    } else if (triggers[i].trigger_max) {
      uint8_t sid = triggers[i].sensorid;
      uint8_t field = triggers[i].field;
      enum ft tp = sensors[sid].fieldtypes[field];
      if (tp == FT_FLOAT) {
        if (sensors[sid].data[field].f <= triggers[i].data_max.f)
	  return true;
      } else if (tp == FT_INT) {
        if (sensors[sid].data[field].i <= triggers[i].data_max.i)
	  return true;
      }
    } else {
    }
  }
  return false;
}

int Control::measureDelay(uint8 sid, time_t ts) {
  return sensors[sid].mdelay;
}

bool Control::SetStartCondition() {
}

void Control::Start() {
  manual_stop = false;
  manual_start = true;
}

void Control::Stop() {
  manual_stop = true;
}
