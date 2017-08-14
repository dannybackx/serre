/*
 * Copyright (c) 2017 Danny Backx
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
#ifndef _INCLUDE_SUNSET_H_
#define _INCLUDE_SUNSET_H_

#include <ELClient.h>
#include <ELClientRest.h>

class Sunset {
public:
  Sunset();
  ~Sunset();
  void query();
  void query(char *lat, char *lon);
  void loop(time_t);

private:
  ELClientRest *rest;
  time_t String2DateTime(char *);
  time_t String2Time(char *);
  byte daysInMonth(int yr, int m);
  bool isLeapYear(int yr);
  char *findData(char *, const char *);

  time_t sunrise, sunset, twilight_begin, twilight_end;
  void DebugPrint(const char *, time_t, const char *);
};
#endif
