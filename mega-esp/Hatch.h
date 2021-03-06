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
#ifndef _INCLUDE_HATCH_H_
#define _INCLUDE_HATCH_H_

#include "item.h"
#include "SimpleL298.h"

class Hatch {
public:
  Hatch();
  Hatch(char *);
  ~Hatch();
  int loop(int, int, int);
  void setSchedule(const char *);
  char *getSchedule();		// Caller must free result
  void set(int);
  void setMotor(int n);
  void setMotor(int a = 2, int b = 3, int c = 9);
  int moving();

  void setMaxTime(int m);
  int getMaxTime();

  void Up(int hr, int mn, int sec, char *msg = NULL);
  void Up(time_t, char *msg = NULL);
  void Down(int hr, int mn, int sec, char *msg = NULL);
  void Down(time_t, char *msg = NULL);
  void Stop(int hr, int mn, int sec, char *msg = NULL);
  void reset();

  void IsUp(char *);
  void IsDown(char *);
  int getPosition();

private:
  int nitems;
  int maxtime;			// Don't run any longer than this amount of seconds
  item *items;
  void PrintSchedule();

  int _moving;			// -1 is going down, +1 is going up, 0 is off
  int _position;		// -1 is down, 0 is moving or unknown, 1 is up
  SimpleL298 *motor;

  int starttime;
  void SetStartTime(int hr, int mn, int sec);
  bool RunTooLong(int hr, int mn, int sec);

  // initialPosition() is private so we call it in the first call of loop()
  // after everything is initialized
  void initialPosition();
  bool initialized;
};
#endif
