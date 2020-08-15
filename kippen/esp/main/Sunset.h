/*
 * Copyright (c) 2017, 2020 Danny Backx
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

#include "Light.h"

// #define       WU_TIME_OK      90
#define  WU_TIME_OK      10
// #define       WU_TIME_FAIL    15
#define  WU_TIME_FAIL    2

class Sunset {
public:
  Sunset();
  ~Sunset();
  void query();
  void query(const char *lat, const char *lon, char *msg = NULL);
  enum lightState loop(time_t);
  void reset();
  void getSchedule(char *buffer, int buflen);

private:
  // State variables
  int sunrise, sunset, twilight_begin, twilight_end;		// These are time-only, no date
  time_t last_call;						// Date of last update
  char *lat, *lon;						// Use this for daily update
  enum lightState stable;					// Stable state

  // Function definitions
  time_t String2DateTime(char *);
  time_t String2Time(char *);
  char *Time2String(time_t);
  byte daysInMonth(int yr, int m);
  bool isLeapYear(int yr);
  char *findData(char *, const char *);
  void DebugPrint(const char *, time_t, const char *);
  int TimeOnly(const char *s);				// pick just hour and minute

  // Internal stuff
  const char *sunset_tag = "Sunset";

  esp_http_client_handle_t	http_client;
  esp_http_client_config_t	http_config;

  const int buflen = 2000;
  char *buf;
  int	the_delay;

  time_t		last_query;
  static const char	*pattern;
  bool			last_error;	// Show debug query if previous one failed

  static const int	normal_delay = WU_TIME_OK * 60;	// Wait time between successful queries
  static const int	error_delay = WU_TIME_FAIL * 60;// Time to wait after error

  static const int	delay_queries = 25 * 60 * 60;
};

extern Sunset *sunset;
#endif
