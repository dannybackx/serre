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
 * This contains code to query the API server at api.sunrise-sunset.org
 *
 * See http://www.sunrise-sunset.org
 *
 * Example call : http://api.sunrise-sunset.org/json?lat=36.7201600&lng=-4.4203400
 *
 * Example reply (spacing added for clarity) :
 *	{"results":
 *	  {
 *	    "sunrise":"5:24:07 AM",
 *	    "sunset":"7:23:50 PM",
 *	    "solar_noon":"12:23:58 PM",
 *	    "day_length":"13:59:43",
 *	    "civil_twilight_begin":"4:55:40 AM",
 *	    "civil_twilight_end":"7:52:16 PM",
 *	    "nautical_twilight_begin":"4:20:59 AM",
 *	    "nautical_twilight_end":"8:26:58 PM",
 *	    "astronomical_twilight_begin":"3:43:42 AM",
 *	    "astronomical_twilight_end":"9:04:15 PM"
 *	   },
 *	   "status":"OK"
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
char *ss_cmd = "/json?lat=36.7201600&lng=-4.4203400";
char *ss_template = "/json?lat=%s&lng=%s";

static char *buf = 0;
static int bufsiz = 512;

Sunset::Sunset() {
  lasttime = -1;
  delta = 600;				// FIXME 10 minutes
  rest = new ELClientRest(&esp);
}

Sunset::~Sunset() {
  delete rest;
  rest = 0;
}

void Sunset::query(char *lat, char *lon) {
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
  rest->get(ss_cmd);

  buf = (char *)malloc(bufsiz);
  buf[0] = 0;
  err = rest->waitResponse(buf, bufsiz);
  if (err == HTTP_STATUS_OK) {
    Serial.print("Sunset success "); Serial.println(buf);
  } else if (err == 0) {
    // timeout
    Serial.print("Sunset timeout "); Serial.println(buf);
  } else {
    Serial.print("Sunset GET failure" ); Serial.println(err);
  }
  free(buf);
  buf = 0;
}

void Sunset::query() {
  query("36.7201600", "-4.4203400");
}
