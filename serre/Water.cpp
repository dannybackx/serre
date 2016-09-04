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
#include "Water.h"
#include "global.h"

Water::Water() {
  items = NULL;
  nitems = 0;
  state = 0;
}

Water::Water(char *desc) {
  items = NULL;
  nitems = 0;
  state = 0;
  setSchedule(desc);
}

Water::~Water() {
  if (items != NULL) {
    free(items);
    items = NULL;
    nitems = 0;
  }
}

int Water::loop(int hr, int mn) {
  for (int i=0; i<nitems; i++) {
    if (items[i].hour == hr && items[i].minute == mn) {
      state = items[i].state;
      if (verbose & VERBOSE_WATER) {
        if (state != 0 && state != 1) {
	  char t[80];
          sprintf(t, "## State %d, i %d, hr %d min %d\n", state, i, hr, mn);
	  Serial.print(t);
        }
      }
      if (verbose & VERBOSE_WATER) {
        Serial.printf("Water(%d,%d) : change ix %d (%d,%d) %d -> %d\n",
	  hr, mn, i, items[i].hour, items[i].minute, state, items[i].state);
      }
      state = items[i].state;
      return state;
    }
  }
  return state;
}

extern "C" {
  void Debug(char *);
  void DebugInt(int);
}

void Water::setSchedule(char *desc) {
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

char *Water::getSchedule() {
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

void Water::set(int s) {
  state = s;
}
