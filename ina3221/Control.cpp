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
#include <ArduinoJson.h>

Control *control;

Control::Control() {
  control = this;
  nalloc = 0;
  next = 0;
  nsensors = 0;
  registering_from = 0;

  manual_stop = timed_stop = false;
  manual_start = timed_start = triggered_start = false;
  amount = 0;
  
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

  // Test code
  triggers[0].sensorid = 0;
  triggers[0].field = 1;
  triggers[0].trigger_min = true;
  triggers[0].data_min.f = 1.23;
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
  amount++;
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

void Control::StartRegistering(uint8 sid) {
  if (registering_from != 0)
    return;
  registering_from = time(0);
}

bool Control::isRegistering(uint8 sid) {
  time_t now = time(0);

  // Should we stop ?
  for (int i=0; i<MAX_TRIGGERS; i++) {
    if (stoppers[i].st_tp == ST_NONE)
      continue;
    else if (stoppers[i].st_tp == ST_TIMER) {
      time_t delta = now - registering_from;
      if (delta > stoppers[i].amount) {
        timed_stop = true;
	return false;
      }
    } else if (stoppers[i].st_tp == ST_AMOUNT) {
      if (amount > stoppers[i].amount) {
        timed_stop = true;
	return false;
      }
    } else {
      // Should not happen
    }
  }

  // Global or per sensor override
  if (manual_stop || timed_stop)
    return false;
  if (manual_start || triggered_start || timed_start) {
    StartRegistering(sid);
    return true;
  }

  for (int i=0; i<MAX_TRIGGERS; i++) {
    if (triggers[i].trigger_min) {
      uint8_t sid = triggers[i].sensorid;
      uint8_t field = triggers[i].field;
      enum ft tp = sensors[sid].fieldtypes[field];
      if (tp == FT_FLOAT) {
        if (sensors[sid].data[field].f >= triggers[i].data_min.f) {
	  StartRegistering(sid);
	  return true;
	}
      } else if (tp == FT_INT) {
        if (sensors[sid].data[field].i >= triggers[i].data_min.i) {
	  StartRegistering(sid);
	  return true;
	}
      }
    } else if (triggers[i].trigger_max) {
      uint8_t sid = triggers[i].sensorid;
      uint8_t field = triggers[i].field;
      enum ft tp = sensors[sid].fieldtypes[field];
      if (tp == FT_FLOAT) {
        if (sensors[sid].data[field].f <= triggers[i].data_max.f) {
	  StartRegistering(sid);
	  return true;
	}
      } else if (tp == FT_INT) {
        if (sensors[sid].data[field].i <= triggers[i].data_max.i) {
	  StartRegistering(sid);
	  return true;
	}
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
  manual_stop = timed_stop = false;
  manual_start = true;
}

void Control::Stop() {
  manual_stop = true;
}

const char *Control::FieldType(enum ft t) {
  switch (t) {
  case FT_NONE:		return "none";
  case FT_FLOAT:	return "float";
  case FT_INT:		return "int";
  case FT_PIN:		return "pin";
  }
}

const char *Control::WriteConfig() {
  DynamicJsonDocument json(1024);

  JsonArray tr = json.createNestedArray("triggers");

  for (int i=0; i<MAX_TRIGGERS; i++) {
    uint8_t sid = triggers[i].sensorid;
    uint8_t field = triggers[i].field;
    enum ft tp = sensors[sid].fieldtypes[field];
    
    const char *sn = sensors[sid].name;
    const char *fn = sensors[sid].fields[field];

    if (triggers[i].trigger_min) {
      tr[i]["trigger"] = "min";
      tr[i]["sensor"] = sn;
      tr[i]["field"] = fn;
      tr[i]["type"] = FieldType(tp);

      if (sensors[sid].fieldtypes[field] == FT_FLOAT)
        tr[i]["value"] = triggers[i].data_min.f;
      else if (sensors[sid].fieldtypes[field] == FT_INT)
        tr[i]["value"] = triggers[i].data_min.i;
    } else if (triggers[i].trigger_max) {
      tr[i]["trigger"] = "max";
      tr[i]["sensor"] = sn;
      tr[i]["field"] = fn;
      tr[i]["type"] = FieldType(tp);

      if (sensors[sid].fieldtypes[field] == FT_FLOAT)
        tr[i]["value"] = triggers[i].data_max.f;
      else if (sensors[sid].fieldtypes[field] == FT_INT)
        tr[i]["value"] = triggers[i].data_max.i;
    }
  }

  JsonArray st = json.createNestedArray("stoppers");

  for (int i=0; i<MAX_TRIGGERS; i++) {
    if (stoppers[i].st_tp == ST_TIMER) {
      st[i]["trigger"] = "timer";
      st[i]["amount"] = stoppers[i].amount;
    } else if (stoppers[i].st_tp == ST_AMOUNT) {
      st[i]["trigger"] = "amount";
      st[i]["amount"] = stoppers[i].amount;
    }
  }

  int len = measureJson(json);
  char *buf = (char *)malloc(len+1);
  serializeJson(json, buf, len+1);
  buf[len] = 0;

  return buf;
}

void Control::ReadConfig(const char *j) {
}
