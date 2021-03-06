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
#include <html.h>

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
    for (int j=0; j<MAX_FIELDS; j++) {
      sensors[i].fieldtypes[j] = FT_NONE;
      sensors[i].fields[j] = 0;
    }
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
  if (nsensors == MAX_SENSORS)
    return -1;
  sensors[nsensors].name = name;
  sensors[nsensors].fn = 0;
  for (int i=0; i<MAX_FIELDS; i++) {
    sensors[nsensors].fields[i] = 0;
    sensors[nsensors].fieldtypes[i] = FT_NONE;
  }
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

const char *Control::stopperType2String(stopper_t t) {
  switch (t) {
  case ST_NONE:		return "none";
  case ST_TIMER:	return "timer";
  case ST_AMOUNT:	return "amount";
  }
}

void Control::describeStopper(ESP8266WebServer *ws, uint8_t i) {
  char v[64];
  snprintf(v, sizeof(v), "<td>%d</td><td>", i);
  ws->sendContent(v);
  stopperTypeDropdown(ws, i, stoppers[i].st_tp);
  snprintf(v, sizeof(v), "</td><td><input name=\"stopper-%d\" type=\"text\" value=\"%d\"> </td>", i, stoppers[i].amount);
  ws->sendContent(v);
}

void Control::stopperTypeDropdown(ESP8266WebServer *ws, uint8_t sti, stopper_t t) {
  char l[80];
  snprintf(l, sizeof(l), webpage_stopper_dropdown_start_format, sti, sti);
  ws->sendContent(l);

  for (int ii=0; ii < (int)ST_LAST; ++ii) {
    stopper_t i = (stopper_t) ii;

    if (t == i)
      snprintf(l, sizeof(l), webpage_stopper_dropdown_format_selected, stopperType2String(i), stopperType2String(i));
    else
      snprintf(l, sizeof(l), webpage_stopper_dropdown_format, stopperType2String(i), stopperType2String(i));
    ws->sendContent(l);
  }

  ws->sendContent(webpage_stopper_dropdown_end);
}

/*
 * Build a dropdown with sensor/field combination, with specified items already preselected
 */
void Control::sensorFieldDropdown(ESP8266WebServer *ws, uint8_t i, const char *sensor, const char *field) {
  char l[120];

  snprintf(l, sizeof(l), webpage_sensor_dropdown_start_format, i, i);
  ws->sendContent(l);

  for (int sid=0; sid<MAX_SENSORS; sid++)
    if (sensors[sid].name != 0) {
      for (int fid=0; fid<MAX_FIELDS; fid++)
        if (sensors[sid].fields[fid] != 0) {
	  char comb[30];
	  snprintf(comb, sizeof(comb), "%s - %s", sensors[sid].name, sensors[sid].fields[fid]);

	  if (strcmp(sensors[sid].name, sensor) == 0 && strcmp(sensors[sid].fields[fid], field) == 0)
	    snprintf(l, sizeof(l), webpage_sensor_dropdown_format_selected, comb, comb);
	  else
	    snprintf(l, sizeof(l), webpage_sensor_dropdown_format, comb, comb);
	  ws->sendContent(l);
      }
    }

  ws->sendContent(webpage_sensor_dropdown_end);
}

const char *Control::triggerType2String(enum tt t) {
  switch (t) {
  case TT_NONE:		return "none";
  case TT_MIN:		return "Min";
  case TT_MAX:		return "Max";
  case TT_MINMAX:	return "MinMax";
  }
}

/*
 * Create a dropdown menu for trigger tri, with default value t
 */
void Control::triggerTypeDropdown(ESP8266WebServer *ws, uint8_t tri, enum tt t) {
  char l[80];
  snprintf(l, sizeof(l), webpage_triggertype_dropdown_start_format, tri, tri);
  ws->sendContent(l);

  for (int ii=0; ii < (int)TT_LAST; ++ii) {
    enum tt i = (enum tt) ii;

    if (t == i)
      snprintf(l, sizeof(l), webpage_triggertype_dropdown_format_selected, triggerType2String(i), triggerType2String(i));
    else
      snprintf(l, sizeof(l), webpage_triggertype_dropdown_format, triggerType2String(i), triggerType2String(i));
    ws->sendContent(l);
  }

  ws->sendContent(webpage_triggertype_dropdown_end);
}

void Control::describeTrigger(ESP8266WebServer *ws, uint8_t i) {
  char v[80];
  uint8_t sid = triggers[i].sensorid;
  uint8_t field = triggers[i].field;
  enum ft tp = sensors[sid].fieldtypes[field];
    
  const char *sn = sensors[sid].name;
  const char *fn = sensors[sid].fields[field];

  enum tt ttv = TT_NONE;
  if (triggers[i].trigger_min && triggers[i].trigger_max)
    ttv = TT_MINMAX;
  else if (triggers[i].trigger_min)
    ttv = TT_MIN;
  else if (triggers[i].trigger_max)
    ttv = TT_MAX;

  snprintf(v, sizeof(v), "<td>%d</td>", i);
  ws->sendContent(v);

  ws->sendContent("<td>");
  triggerTypeDropdown(ws, i, ttv);
  ws->sendContent("</td><td>");
  sensorFieldDropdown(ws, i, sn, fn);

  // Min. value
  v[0] = 0;
  if (sensors[sid].fieldtypes[field] == FT_FLOAT)
    snprintf(v, sizeof(v), "</td><td><input name=\"min-%d\" type=\"text\" value=\"%f\"> </td>", i, triggers[i].data_min.f);
  else if (sensors[sid].fieldtypes[field] == FT_INT)
    snprintf(v, sizeof(v), "</td><td><input name=\"min-%d\" type=\"text\" value=\"%d\"> </td>", i, triggers[i].data_min.i);
  ws->sendContent(v);

  // Max. value
  v[0] = 0;
  if (sensors[sid].fieldtypes[field] == FT_FLOAT) {
    snprintf(v, sizeof(v), "<td><input name=\"max-%d\" type=\"text\" value=\"%f\"> </td>", i, triggers[i].data_max.f);
  } else if (sensors[sid].fieldtypes[field] == FT_INT) {
    snprintf(v, sizeof(v), "<td><input name=\"max-%d\" type=\"text\" value=\"%d\"> </td>", i, triggers[i].data_max.i);
  }
  ws->sendContent(v);
  ws->sendContent("\n");
}

/*
 * Caller must free return value
 *
 * Sample output : {"triggers":[{"trigger":"min","sensor":"AHT10","field":"humidity","value":1.23}],"stoppers":[]}
 */
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

      if (sensors[sid].fieldtypes[field] == FT_FLOAT)
        tr[i]["value"] = triggers[i].data_min.f;
      else if (sensors[sid].fieldtypes[field] == FT_INT)
        tr[i]["value"] = triggers[i].data_min.i;
    } else if (triggers[i].trigger_max) {
      tr[i]["trigger"] = "max";
      tr[i]["sensor"] = sn;
      tr[i]["field"] = fn;

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

void Control::ReadConfig(const char *jtxt) {
  DynamicJsonDocument	json(1024);

  DeserializationError e = deserializeJson(json, jtxt);
  if (e) {
    return;
  }

  JsonArray tr = json["triggers"];
  for (int i=0; i<MAX_TRIGGERS; i++) {
    if (tr[i].isNull())
      continue;
    const char *t = tr[i]["trigger"];
    // FIX ME trigger type can have multiple values, could be both MIN and MAX
    triggers[i].trigger_min = false;
    triggers[i].trigger_max = false;
    if (strcmp(t, "min") == 0)
      triggers[i].trigger_min = true;
    else if (strcmp(t, "max") == 0)
      triggers[i].trigger_max = true;
    // end FIX ME
    const char *sn = tr[i]["sensor"];
    int sid = GetSensor(sn);
    if (sid < 0)
      continue;
    triggers[i].sensorid = sid;
    const char *field = tr[i]["field"];
    int fn = GetField(sid, field);
    if (fn < 0)
      continue;
    triggers[i].field = fn;
    enum ft fte = sensors[sid].fieldtypes[fn];
    switch (fte) {
    case FT_INT:
      triggers[i].data_min.i = tr[i]["value"];

      Serial.printf("ReadConfig trigger %d -> %s, sensor %s (%d), field %s (%d), value %d\n",
        i, (t == 0) ? "(null)" : t,
        sn, sid, field, fn, triggers[i].data_min.i);
      break;
    case FT_FLOAT:
      triggers[i].data_min.f = tr[i]["value"];

      Serial.printf("ReadConfig trigger %d -> %s, sensor %s (%d), field %s (%d), value %f\n",
        i, (t == 0) ? "(null)" : t,
        sn, sid, field, fn, triggers[i].data_min.f);
      break;
    }
  }

  JsonArray st = json["stoppers"];
  for (int i=0; i<MAX_TRIGGERS; i++) {
    if (st[i].isNull())		// FIX ME not sure why we need to break instead of continue
      break;

    const char *t = st[i]["trigger"];
    if (strcasecmp(t, "timer") == 0)
      stoppers[i].st_tp = ST_TIMER;
    else if (strcasecmp(t, "amount") == 0)
      stoppers[i].st_tp = ST_AMOUNT;
    stoppers[i].amount = st[i]["amount"];
  }
}

int Control::GetSensor(const char *name) {
  if (name == 0)
    return -1;
  for (int i=0; i<MAX_SENSORS; i++)
    if (strcasecmp(name, sensors[i].name) == 0)
      return i;
  return -1;
}

int Control::GetField(uint8_t sid, const char *fname) {
  if (fname == 0)
    return -1;
  for (int i=0; i<MAX_FIELDS; i++)
    if (strcasecmp(fname, sensors[sid].fields[i]) == 0)
      return i;
  return -1;
}
