/*
 * Copyright (c) 2016, 2017, 2020 Danny Backx
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

#include "Kippen.h"
#include "Hatch.h"
#include "SimpleL298.h"

Hatch::Hatch() {
  items = NULL;
  nitems = 0;
  _moving = 0;
  _position = 0;	// Don't know
  maxtime = starttime = 0;
  initialized = false;

  motor = 0;
}

#if 0
Hatch::Hatch(char *desc) {
  items = NULL;
  nitems = 0;
  _moving = 0;
  _position = 0;
  maxtime = starttime = 0;
  motor = 0;
  initialized = false;
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

void Hatch::SetStartTime(time_t st) {
  starttime = st;
}

#if 0
void Hatch::SetStartTime(int hr, int mn, int sec) {
  starttime = hr * 3600 + mn * 60 + sec;
}

bool Hatch::RunTooLong(int hr, int mn, int sec) {
  int n = hr * 3600 + mn * 60 + sec;
  if (n - starttime > maxtime)
    return true;
  return false;
}
#endif

bool Hatch::RunTooLong(time_t ts) {
  if (ts - starttime > maxtime)
    return true;
  return false;
}

int Hatch::loop(time_t now) {
  char *msg;
  initialPosition();

  // Stop if running for too long
  if (_moving != 0) {
    if (RunTooLong(now)) {
      // Assume we're up/down
      if (_moving < 0) {
        msg = "Run Too Long : Hatch is down";
        _position = -1;
      } else if (_moving > 0) {
        msg = "Run Too Long : Hatch is up";
	_position = +1;
      }

      Stop(now, msg);
      return _moving;
    }
  }

  // If we're moving, stop if we hit the right sensor
  if (_moving < 0) {
    if (CONFIG_SENSOR_DOWN_PIN >= 0) {
      int state = digitalRead(CONFIG_SENSOR_DOWN_PIN);
      if (state == 0) {
	_position = -1;
	Stop(now, "Down sensor : stop hatch");
      }
    }
    return _moving;
  } else if (_moving > 0) {
    if (CONFIG_SENSOR_UP_PIN >= 0) {
      int state = digitalRead(CONFIG_SENSOR_UP_PIN);
      if (state == 0) {
	_position = +1;
	Stop(now, "Up sensor : stop hatch");
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
	// Serial.print(__LINE__); Serial.print(" Down()");
        Down(hr, mn, sec);
	SetStartTime(hr, mn, sec);
	return _moving;
      case +1:
	// Serial.print(__LINE__); Serial.print(" Up()");
        Up(hr, mn, sec);
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
#if 0
int Hatch::loop(int hr, int mn, int sec) {
  char *msg;
  initialPosition();

  // Stop if running for too long
  if (_moving != 0) {
    if (RunTooLong(hr, mn, sec)) {
      // Assume we're up/down
      if (_moving < 0) {
        msg = "Run Too Long : Hatch is down";
        _position = -1;
      } else if (_moving > 0) {
        msg = "Run Too Long : Hatch is up";
	_position = +1;
      }

      Stop(hr, mn, sec, msg);
      return _moving;
    }
  }

  // If we're moving, stop if we hit the right sensor
  if (_moving < 0) {
    if (CONFIG_SENSOR_DOWN_PIN >= 0) {
      int state = digitalRead(CONFIG_SENSOR_DOWN_PIN);
      if (state == 0) {
	_position = -1;
	Stop(hr, mn, sec, "Down sensor : stop hatch");
      }
    }
    return _moving;
  } else if (_moving > 0) {
    if (CONFIG_SENSOR_UP_PIN >= 0) {
      int state = digitalRead(CONFIG_SENSOR_UP_PIN);
      if (state == 0) {
	_position = +1;
	Stop(hr, mn, sec, "Up sensor : stop hatch");
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
	// Serial.print(__LINE__); Serial.print(" Down()");
        Down(hr, mn, sec);
	SetStartTime(hr, mn, sec);
	return _moving;
      case +1:
	// Serial.print(__LINE__); Serial.print(" Up()");
        Up(hr, mn, sec);
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
#endif

void Hatch::setSchedule(const char *desc) {
  const char *p;
  int count;

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

  // Convert CSV string into items
  p = desc;
  for (int i=0; i<nitems; i++) {
    int hr, min, val, nchars;
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

void Hatch::setMotor(int a, int b, int c) {
  motor = new SimpleL298(a, b, c);
  motor->setSpeed(200);
  motor->run(RELEASE);
}

int Hatch::moving() {
  return _moving;
}

void Hatch::Up(time_t ts, char *msg) {
  int hr, min, sec;
  hr = hour(ts); min = minute(ts); sec = second(ts);
  Up(hr, min, sec, msg);
}

void Hatch::Up(int hr, int mn, int sec, char *msg) {
  char b[40];
  sprintf(b, "Hatch moving up %02d:%02d:%02d\n", hr, mn, sec);
  Serial.print(b);

  SetStartTime(hr, mn, sec);

  if (_position == 1)
    return;
  if (_moving)
    return;
  motor->run(BACKWARD);
  _moving = +1;
  // ts->changeState(hr, mn, sec, _moving, _position, msg);
}

void Hatch::Down(time_t ts, char *msg) {
  int hr, min, sec;
  hr = hour(ts); min = minute(ts); sec = second(ts);
  Down(hr, min, sec, msg);
}

void Hatch::Down(int hr, int mn, int sec, char *msg) {
  char b[40];
  sprintf(b, "Hatch moving down %02d:%02d:%02d ", hr, mn, sec);
  Serial.print(b);
  if (msg) Serial.print(msg);
  Serial.println();

  SetStartTime(hr, mn, sec);

  if (_position == -1) {
    Serial.println("Hatch::Down position already -1");
    return;
  }
  if (_moving) {
    Serial.println("Hatch::Down already _moving");
    return;
  }
  motor->run(FORWARD);
  _moving = -1;
  // ts->changeState(hr, mn, sec, _moving, _position, msg);
}

void Hatch::Stop(int hr, int mn, int sec, char *msg) {
  if (_moving == 0)
    return;

  char b[40];
  Serial.print("Hatch stopped (");
  if (msg) Serial.print(msg);
  sprintf(b, ") %02d:%02d:%02d\n", hr, mn, sec);
  Serial.println(b);

  motor->run(RELEASE);
  _moving = 0;
  // ts->changeState(hr, mn, sec, _moving, _position, msg);
}

void Hatch::reset() {
  _position = 0;
}

void Hatch::IsUp(char *msg) {
  char buffer[80];
  sprintf(buffer, "Hatch is up (%s)", msg);
 
  kippen->Report(buffer);

  _position = 1;
}

void Hatch::IsDown(char *msg) {
  char buffer[80];
  sprintf(buffer, "Hatch is down (%s)", msg);
  kippen->Report(buffer);

  _position = -1;
}

int Hatch::getPosition() {
  return _position;
}

void Hatch::initialPosition() {
  if (initialized)
    return;
  initialized = true;

  if (_position < 0 || _position > 0) {
    char buffer[60];
    sprintf(buffer, "Hatch initial position, position %d\n", _position);
    Serial.print(buffer);
    return;
  }

  // Serial.print("Hatch initial position");

  time_t nowts = now();
#if 0
  enum lightState sun = sunset->loop(nowts);

  switch (sun) {
  case LIGHT_MORNING:
  case LIGHT_DAY:
  Serial.println(" : moving up");
    Up(nowts, "initialPosition");
    break;
  case LIGHT_EVENING:
  case LIGHT_NIGHT:
  Serial.println(" : moving down");
    Down(nowts), "initialPosition";
    break;
  default:
    Serial.println(" : no action");
    break;
  }
#else
  #warning "no init based on time"
#endif
}
#endif
