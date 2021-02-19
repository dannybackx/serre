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
  FT_INT
};

#define	MAX_SENSORS	5
#define	MAX_FIELDS	3

struct sensorid {
  char		*name;
  char		*fields[MAX_FIELDS];
  ft		fieldtypes[MAX_FIELDS];
};

struct sensordata {
  uint8		sensorid;
  time_t	ts;
  union {
    float	f;
    int32_t	i;
  }		data[MAX_FIELDS];
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
  ft getType(int sensor, int field);
  float getDataFloat(int ix, int sensor, int field);
  int getDataInt(int ix, int sensor, int field);

private:
  int nalloc;
  int next;
  bool AllocateMemory();
  
  struct sensorid sensors[MAX_SENSORS];
  struct sensordata *data;
};

extern Control *control;
