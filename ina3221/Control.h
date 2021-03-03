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

#include <Arduino.h>

enum ft {
  FT_NONE,
  FT_FLOAT,
  FT_INT,
  FT_PIN
};

#define	MAX_SENSORS	8
#define	MAX_FIELDS	4
#define	MAX_TRIGGERS	4

struct sensorid {
  const char	*name;
  const char	*fields[MAX_FIELDS];
  ft		fieldtypes[MAX_FIELDS];
  int		fn;

  // Data for decision making
  time_t	ts;
  union {
    float	f;
    int32_t	i;
  }		data[MAX_FIELDS];

  int		mdelay;
};

struct sensordata {
  uint8		sensorid;
  time_t	ts;
  union {
    float	f;
    int32_t	i;
  }		data[MAX_FIELDS];
};

// Trigger = turn on at any of these
struct trigger {
  uint8_t	sensorid;
  uint8_t	field;
  union {
    float	f;
    int32_t	i;
  }		data_min, data_max;
  bool		trigger_min, trigger_max;
};

enum stopper_t {
  ST_NONE,
  ST_TIMER,
  ST_AMOUNT
};

// Stop conditions
struct stopper {
  stopper_t	st_tp;
  int		amount;
};

class Control {
public:
  Control();
  ~Control();

  bool SetStartCondition();
  void Start();
  void Stop();
  void LogData();
  int RegisterSensor(const char *name);
  void SensorRegisterField(int sensor, char *field, ft field_type);

  void RegisterData(int sensor, time_t ts);
  void RegisterData(int sensor, int field, float value);
  void RegisterData(int sensor, int field, int value);

  bool setAllocation(int num);
  int getAllocation();

  time_t getTimestamp(int ix);
  int getDataSensor(int ix);
  ft getFieldType(int sensor, int field);
  const char *getSensorName(int sensor);
  const char *getFieldName(int sensor, int field);
  float getDataFloat(int ix, int field);
  int getDataInt(int ix, int field);

  bool isRegistering(uint8 sid, time_t ts, float a, float b = 0.0, float c = 0.0, float d = 0.0);
  bool isRegistering(uint8 sid, time_t ts, uint32 a, uint32 b = 0, uint32 c = 0, uint32 d = 0);
  int measureDelay(uint8 sid, time_t ts);

  const char *WriteConfig();
  void ReadConfig(const char *);

private:
  int		nalloc;
  int		next;
  int		nsensors;
  bool		manual_start, manual_stop;
  bool		timed_start, timed_stop;
  bool		triggered_start;
  time_t	registering_from;
  int		amount;

  bool AllocateMemory();
  
  struct sensorid sensors[MAX_SENSORS];
  struct sensordata *data;

  // Start conditions
  struct trigger triggers[MAX_TRIGGERS];
  // Stop conditions
  struct stopper stoppers[MAX_TRIGGERS];

  void sensorData(uint8 sid, time_t ts, float a, float b = 0.0, float c = 0.0, float d = 0.0);
  void sensorData(uint8 sid, time_t ts, uint32 a, uint32 b = 0, uint32 c = 0, uint32 d = 0);
  bool isRegistering(uint8 sid);
  void StartRegistering(uint8_t sid);
  const char *FieldType(enum ft);
};

extern Control *control;
