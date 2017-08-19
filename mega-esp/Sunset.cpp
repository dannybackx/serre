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
 *
 * This contains code to query the API server at api.sunrise-sunset.org .
 * Note this needs a patch to esp-link/el-client to cope with reply length ~500 bytes.
 *
 * See http://www.sunrise-sunset.org
 *
 * Example call : http://api.sunrise-sunset.org/json?lat=36.7201600&lng=-4.4203400&formatted=0
 *
 * Example reply (spacing added for clarity) :
 *	{
 *	  "results":
 *	  {
 *	    "sunrise":"2015-05-21T05:05:35+00:00",
 *	    "sunset":"2015-05-21T19:22:59+00:00",
 *	    "solar_noon":"2015-05-21T12:14:17+00:00",
 *	    "day_length":51444,
 *	    "civil_twilight_begin":"2015-05-21T04:36:17+00:00",
 *	    "civil_twilight_end":"2015-05-21T19:52:17+00:00",
 *	    "nautical_twilight_begin":"2015-05-21T04:00:13+00:00",
 *	    "nautical_twilight_end":"2015-05-21T20:28:21+00:00",
 *	    "astronomical_twilight_begin":"2015-05-21T03:20:49+00:00",
 *	    "astronomical_twilight_end":"2015-05-21T21:07:45+00:00"
 *	  },
 *	  "status":"OK"
 *	}
 */
#include <Arduino.h>

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientRest.h>
#include <ELClientMqtt.h>

#include "SFE_BMP180.h"
#include "TimeLib.h"
#include "Light.h"
#include "Hatch.h"
#include "ThingSpeak.h"
#include "Sunset.h"
#include "global.h"

extern ELClient esp;

char *ss_url = "api.sunrise-sunset.org";
char *ss_template = "/json?lat=%s&lng=%s&formatted=0";

static char *buf = 0;
static int bufsiz = 700;

Sunset::Sunset() {
  rest = 0;	// Don't do more here, keep CTOR and rest->begin() together
  today = 0;
  stable = LIGHT_NONE;
}

Sunset::~Sunset() {
  delete rest;
  rest = 0;
}

void Sunset::query(char *lat, char *lon) {
  this->lat = lat;
  this->lon = lon;

  rest = new ELClientRest(&esp);
  if (rest == 0) {
    Serial.println("Sunset : could not create REST api");
    return;
  }
  int err = rest->begin(ss_url);
  if (err != 0) {
    delete rest;
    rest = 0;

    Serial.print("Sunset : could not initialize ");
    Serial.print(ss_url);
    Serial.print(" , error code ");
    Serial.println(err);
  } else {
    Serial.print("Sunset : initialized ");
    Serial.println(ss_url);
  }

  buf = (char *)malloc(bufsiz);

  sprintf(buf, ss_template, lat, lon);
  rest->get(buf);

  uint16_t totalLen = 0, packetLen = 0;
  memset(buf, 0, bufsiz);
  char *ptr = buf;
  err = rest->waitResponse2(ptr, bufsiz-1, &totalLen, &packetLen);
  while (totalLen) {
    ptr += packetLen;
    err = rest->waitResponse2(ptr, bufsiz-1, &totalLen, &packetLen);
  }
  if (err == HTTP_STATUS_OK) {
    Serial.print("Sunset success ");

    sunrise = String2Time(findData(buf, "sunrise"));
    sunset = String2Time(findData(buf, "sunset"));
    twilight_begin = String2Time(findData(buf, "civil_twilight_begin"));
    twilight_end = String2Time(findData(buf, "civil_twilight_end"));

    DebugPrint("sunrise ", sunrise, ", ");
    DebugPrint("sunset ", sunset, "\n");
  } else if (err == 0) {
    // timeout
    Serial.print("Sunset timeout "); Serial.println(buf);
  } else {
    Serial.print("Sunset GET failure" ); Serial.println(err);
  }
  free(buf);
  buf = 0;
}

void Sunset::DebugPrint(const char *prefix, time_t tm, const char *suffix) {
  char buf[20];
  int	a = tm / 3600L,
  	b = (tm / 60L) % 60L,
	c = tm % 60L;
  sprintf(buf, "%02d:%02d:%02d", a, b, c);

  Serial.print(prefix);
  Serial.print(buf);
  Serial.print(suffix);
}

void Sunset::query() {
  query("36.7201600", "-4.4203400");	// Demo coordinates from their site
}

/*
 * In the "response", find the date/time field after the specified search string.
 * Return a pointer into the original string (no private copy, not terminated).
 * Flag error if we don't see quotes/column in the expected places.
 */
char *Sunset::findData(char *response, const char *search) {
  char *f = strstr(response, search);

  int len = strlen(search);
  if (! f) {
    // Serial.println("error 1");
    return (char *)0;
  }
  if (f[-1] != '"') {
    // Serial.println("error 2");
    return (char *)0;
  }
  if (f[len] != '"') {
    // Serial.println("error 3");
    return (char *)0;
  }
  if (f[len+1] != ':') {
    // Serial.println("error 4");
    return (char *)0;
  }
  if (f[len+2] != '"') {
    // Serial.println("error 5");
    return (char *)0;
  }

  return f + len + 3;
}

/*
 * Convert a standard time string such as
 *	    "sunrise":"2015-05-21T05:05:35+00:00",
 * to the seconds since "the epoch" (January 1, 1970) like on a Unix system.
 */
time_t Sunset::String2DateTime(char *ts) {
  long epoch=0;
  int year, month, day, hour, minute, second;

  sscanf(ts, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  if (year<100) year+=2000;

  for (int yr=1970;yr<year;yr++)
    if (isLeapYear(yr))
      epoch+=366*86400L;
    else
      epoch+=365*86400L;

  for(int m=1;m<month;m++)
    epoch += daysInMonth(year,m)*86400L;
  epoch += (day-1)*86400L;
  epoch += hour*3600L;
  epoch += minute*60;
  epoch += second;

  return epoch;
}

/*
 * Convert a standard time string such as
 *	    "sunrise":"2015-05-21T05:05:35+00:00",
 * to just the seconds since midnight.
 */
time_t Sunset::String2Time(char *ts) {
  int year, month, day, hour, minute, second;

  sscanf(ts, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);

  time_t r = hour * 3600L + minute * 60L + second;
#if 0
  Serial.print("String2Time(");
  char *p = ts;
  char c = *p;
  while (c != '"') {
    Serial.print(c);
    c = *++p;
  }
  Serial.print(") -> "); Serial.println(r);
#endif
  return r;
}

bool Sunset::isLeapYear(int yr)
{
  if (yr % 4 == 0 && yr % 100 != 0 || yr % 400 == 0)
    return true;
  else
    return false;
}

byte Sunset::daysInMonth(int yr, int m)
{
  byte days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  if (m==2 && isLeapYear(yr))
    return 29;
  else
    return days[m-1];
}

/*
 * Query once per day
 */
enum lightState Sunset::loop(time_t t) {
  // First call ever, so we just got initialized.
  // Don't bother querying, this is done from elsewhere.
  if (today == 0) {
    today = t;
    return LIGHT_NONE;
  }

  if ((day(t) != day(today)) || (month(t) != month(today)) || (year(t) != year(today))) {
    query(lat, lon);
    today = t;
  }

  int	h = hour(t), m = minute(t), s = second(t);
  time_t tt = h * 3600L + m * 60L + s;

  if (tt < sunrise) {
    stable = LIGHT_NIGHT;
    return LIGHT_NIGHT;
  }
  if (tt > sunset) {
    if (stable == LIGHT_DAY) {
      stable = LIGHT_NIGHT;
      return LIGHT_EVENING;	// Transient state
    }
    stable = LIGHT_NIGHT;
    return LIGHT_NIGHT;
  }
  if (stable == LIGHT_NIGHT) {
    stable = LIGHT_DAY;
    return LIGHT_MORNING;	// Transient state
  }
  return LIGHT_DAY;
}

void Sunset::reset() {
  today = 123456789L;	// Causes re-query
  Serial.println("Sunset : reset");
}
