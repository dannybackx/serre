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
#include <Arduino.h>

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientRest.h>
#include <ELClientMqtt.h>

#include "TimeLib.h"
#include "Hatch.h"
#include "Light.h"
#include "SFE_BMP180.h"
#include "ThingSpeak.h"
#include "global.h"
#include "AFMotor.h"
#include "SimpleL298.h"

Hatch::Hatch() {
  items = NULL;
  nitems = 0;
  _moving = 0;
  _position = 0;
  maxtime = starttime = 0;

  motor = 0;
}

Hatch::Hatch(char *desc) {
  items = NULL;
  nitems = 0;
  _moving = 0;
  _position = 0;
  maxtime = starttime = 0;
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

void Hatch::PrintSchedule() {
    int i;
    Serial.print("Nitems "); Serial.println(nitems);
    for (i=0; i<nitems; i++) {
      Serial.print("   "); Serial.print(i); Serial.print("  - ");
      Serial.print(items[i].hour);
      Serial.print(":");
      Serial.print(items[i].minute);
      Serial.print(" -> ");
      Serial.println(items[i].state);
    }
}

void Hatch::setMaxTime(int m) {
  maxtime = m;
}

int Hatch::getMaxTime() {
  return maxtime;
}

void Hatch::SetStartTime(int hr, int mn, int sec) {
  starttime = hr * 3600 + mn * 60 + sec;
}

bool Hatch::RunTooLong(int hr, int mn, int sec) {
  int n = hr * 3600 + mn * 60 + sec;
  if (n - starttime > maxtime)
    return true;
  return false;
}

int Hatch::loop(int hr, int mn, int sec) {
  // Serial.print("Hatch "); Serial.print(hr); Serial.print(":"); Serial.print(mn); Serial.print(":"); Serial.print(sec); Serial.print(" moving "); Serial.print(_moving); Serial.print(" state "); Serial.println(_position);
  // delay(500);

  // Stop if running for too long
  if (_moving != 0) {
    if (RunTooLong(hr, mn, sec)) {
      Stop();
      return _moving;
    }
  }

  // If we're moving, stop if we hit the right sensor
  if (_moving < 0) {
    if (sensor_down_pin >= 0) {
      int state = digitalRead(sensor_down_pin);
      if (state == 0) {
	// Serial.println("Down sensor : stop hatch");
	Stop();
      }
    }
    return _moving;
  } else if (_moving > 0) {
    if (sensor_up_pin >= 0) {
      int state = digitalRead(sensor_up_pin);
      if (state == 0) {
	// Serial.println("Up sensor : stop hatch");
	Stop();
      }
    }
    return _moving;
  }

  // Don't activate the hatch after a couple of seconds
  if (sec > 2)
    return _moving;

  // sprintf(reply, "Loop %02d:%02d", hr, mn); Serial.print(reply);
  // Serial.print(" "); Serial.print(nitems);
  // for (int i=0; i<10; i++) Serial.print('');

  // We're not moving. Check against the schedule
  for (int i=0; i<nitems; i++) {
    if (items[i].hour == hr && items[i].minute == mn) {
      switch (items[i].state) {
      case -1:
	Serial.print(__LINE__); Serial.print(" Down()");
        Down();
	SetStartTime(hr, mn, sec);
	return _moving;
      case +1:
	Serial.print(__LINE__); Serial.print(" Up()");
        Up();
	SetStartTime(hr, mn, sec);
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

void Hatch::setSchedule(const char *desc) {
  const char *p;
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

  // PrintSchedule();
}

char *Hatch::getSchedule() {
  int len = 0,
      max = 24;	// initial size, good for 2 entries, increases if necessary
  char *r = (char *)malloc(max), s[10];

  r[0] = '\0';
  for (int i=0; i<nitems; i++) {
    if (i != 0)
      r[len++] = ',';
    r[len] = '\0';
    sprintf(s, "%02d:%02d,%d", items[i].hour, items[i].minute, items[i].state);
    int ms = strlen(s);

    // increase allocation size ?
    if (len + ms > max) {
      max += 10;
      // Note doing this the hard way, realloc() gave strange results
      char *r2 = (char *)malloc(max);
      strcpy(r2, r);
      free(r);
      r = r2;
    }
    strcat(r, s);
    len = strlen(r);
  }
  return r;
}

void Hatch::set(int s) {
  _moving = s;
}

#if 0
void Hatch::setMotor(int n) {
  Serial.print(gpm(hatch_use_motor));
  Serial.print(n);
  Serial.println(".");

  motor = new AF_DCMotor(n);
  motor->setSpeed(200);
  motor->run(RELEASE);
}
#else
void Hatch::setMotor(int a, int b, int c) {
  motor = new SimpleL298(a, b, c);
  motor->setSpeed(200);
  motor->run(RELEASE);
}
#endif

int Hatch::moving() {
  return _moving;
}

void Hatch::Up(int hr, int mn, int sec) {
  SetStartTime(hr, mn, sec);
  Up();
}

void Hatch::Up() {
  // Serial.print(__LINE__); Serial.println("Up()");
  if (_position == 1)
    return;
  if (_moving)
    return;
  motor->run(BACKWARD);
  _moving = +1;
  ts->changeState(_moving);
}

void Hatch::Down(int hr, int mn, int sec) {
  SetStartTime(hr, mn, sec);
  Down();
}

void Hatch::Down() {
  // Serial.print(__LINE__); Serial.println("Down()");
  if (_position == -1)
    return;
  if (_moving)
    return;
  motor->run(FORWARD);
  _moving = -1;
  ts->changeState(_moving);
}

void Hatch::Stop() {
  if (_moving == 0)
    return;
  motor->run(RELEASE);
  _moving = 0;
  ts->changeState(_moving);
}

void Hatch::reset() {
  _position = 0;
}

void Hatch::IsUp() {
  _position = 1;
}

void Hatch::IsDown() {
  _position = -1;
}

int Hatch::getPosition() {
  return _position;
}
