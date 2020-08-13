/*
 * Copyright (c) 2016, 2017 Danny Backx
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
#ifndef _INCLUDE_LIGHT_H_
#define _INCLUDE_LIGHT_H_

enum lightState {
  LIGHT_NONE,		// unknown
  LIGHT_NIGHT,
  LIGHT_MORNING,	// open the door
  LIGHT_DAY,
  LIGHT_EVENING		// close the door
};

class Light {
public:
  Light();
  ~Light();
  enum lightState loop(time_t, enum lightState);
  void setSensorPin(int pin);
  int getSensorPin();
  void setLowTreshold(int);
  int getLowTreshold();
  void setHighTreshold(int);
  int getHighTreshold();
  void setDuration(int);
  int getDuration();
  int query();
  char *Light2String(enum lightState l);

private:
  // Configuration items
  int sensorPin;
  int lowTreshold, highTreshold;
  int duration;

  // Track state
  time_t		stableTime;
  enum lightState	stableValue,
  			currentValue;
};
#endif
