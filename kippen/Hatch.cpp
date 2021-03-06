/*
 * Copyright (c) 2016 Danny Backx
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
#include "Hatch.h"
#include "global.h"
#include "AFMotor.h"

Hatch::Hatch() {
  items = NULL;
  nitems = 0;
  _moving = 0;

  motor = 0;
}

Hatch::Hatch(char *desc) {
  items = NULL;
  nitems = 0;
  _moving = 0;
  motor = 0;
  setSchedule(desc);
}

Hatch::~Hatch() {
  if (items != NULL) {
    free(items);
    items = NULL;
    nitems = 0;
  }

  delete motor;
  motor = 0;
}

int Hatch::loop(int hr, int mn) {
  // If we're moving, stop if we hit the right sensor
  if (_moving < 0) {
    return _moving;
  } else if (_moving > 0) {
    return _moving;
  }

  // We're not moving. Check against the schedule
  for (int i=0; i<nitems; i++) {
    if (items[i].hour == hr && items[i].minute == mn) {
      switch (items[i].state) {
      case -1:
        HatchDown();
	return _moving;
      case +1:
        HatchUp();
        return _moving;
      default:
        ; // No action
      }
      return _moving;
    }
  }
  return _moving;
}

extern "C" {
  void Debug(char *);
  void DebugInt(int);
}

void Hatch::setSchedule(char *desc) {
  char *p;
  int count;

  // Debug("setSchedule("); Debug(desc); Debug(")\n");

  // Count commas
  for (p=desc,count=0; *p; p++)
    if (*p == ',')
      count++;

  // Clean up old allocation, make a new one
  if (nitems != 0) {
    free(items);
    items = NULL;
  }

  nitems = (count + 1) / 2;
  items = (item *)malloc(sizeof(item) * nitems);

  // DebugInt(count); Debug(" -> "); DebugInt(nitems); Debug(" items\n");

  // Convert CSV string into items
  p = desc;
  for (int i=0; i<nitems; i++) {
    int hr, min, val, nchars;
    // Debug("sscanf("); Debug(p); Debug(")\n");
    sscanf(p, "%d:%d,%d%n", &hr, &min, &val, &nchars);

    p += nchars;
    if (*p == ',')
      p++;

    items[i].hour = hr;
    items[i].minute = min;
    items[i].state = val;
  }

#if 0
  Debug("\nProgram :\n");
  for (int i=0; i<nitems; i++) {
    DebugInt(items[i].hour);
    Debug(":");
    DebugInt(items[i].minute);
    Debug(" -> ");
    DebugInt(items[i].state);
    Debug("\n");
  }
  Debug("\n");
#endif
}

char *Hatch::getSchedule() {
  char *r = (char *)malloc(256), s[16];
  int len = 0;
  r[0] = '\0';
  for (int i=0; i<nitems; i++) {
    if (i != 0)
      r[len++] = ',';
    r[len] = '\0';
    sprintf(s, "%02d:%02d,%d", items[i].hour, items[i].minute, items[i].state);
    strcat(r, s);
  }
  return r;
}

void Hatch::set(int s) {
  _moving = s;
}

void Hatch::setMotor(int n) {
  motor = new AF_DCMotor(n);
  motor->run(RELEASE);
}

#if 0
void SpeedTest() {
  int speed = ((count % 100) < 50) ? 255 : 128;

  motor->setSpeed(speed);
  if ((count % 200) < 100)
    motor->run(FORWARD);
  else
    motor->run(BACKWARD);
}
#endif

int Hatch::moving() {
  return _moving;
}

void Hatch::Up() {
  if (_moving)
    return;
  motor->run(BACKWARD);
  _moving = +1;
}

void Hatch::Down() {
  if (_moving)
    return;
  motor->run(FORWARD);
  _moving = -1;
}

void Hatch::reset() {
}
