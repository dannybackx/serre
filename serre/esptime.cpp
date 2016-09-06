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

#include <ESP8266WiFi.h>
#include "DS3231.h"

extern "C" {
#include <sntp.h>
#include <time.h>
#include <user_interface.h>
#include "global.h"
}

#include "esptime.h"

esptime::esptime() {
  init_time = 0;
  init_system_time = 0;
  rtc = 0;
}

void esptime::begin() {
  // Set up real time clock
  Serial.println("Initialize SNTP ...");
  sntp_init();
  sntp_setservername(0, ntp_server_1);
  sntp_setservername(1, ntp_server_2);

  // FIXME
  // This is a bad idea : fixed time zone, no DST processing, .. but it works for now.
  // (void)sntp_set_timezone(+2);
  tzset();

  // DS323 Real time clock
  Serial.print("Initializing RTC ... ");
  RTCInitialize();
  Serial.println(rtc ? "ok" : "failed");

  inited = 1;
}

void esptime::RTCInitialize() {
  rtc = new DS3231();
  char ok = rtc->begin();	// 0 return value is a problem
  if (ok == 0) {
    delete rtc;
    rtc = 0;
  }
}

esptime::~esptime() {
  inited = 0;
}

void esptime::loop() {
}

time_t esptime::now(char *s) {
  time_t t;
  uint32 gt = system_get_time();

  if (init_time == 0 || gt > 0x50000000) {
    // SNTP : wait for a correct time, and report it
    t = sntp_get_current_timestamp();
    while (t < 0x1000) {
      if (s) Serial.print(s);
      delay(1000);
      t = sntp_get_current_timestamp();
    }

    init_time = t;
    init_system_time = system_get_time();
  } else {
    // Avoid network traffic by calling sntp_*() all the time
    uint32 diff = gt - init_system_time;
    // Serial.printf("Diff %d\n", diff);

    t += (diff / 1000000);
  }

  return t;
}
