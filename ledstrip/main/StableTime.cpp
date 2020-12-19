/*
 * Call this module instead of time(0) or gettimeofday(&x, 0) who both freeze occasionally.
 * Currently (with a quick loop()) there's no need to make this complicated.
 *
 * In another case, this could be implemented as one process that periodically does a call
 * to gettimeofday(), and another to monitor whether it freezes.
 *
 * Copyright (c) 2019, 2020 Danny Backx
 *
 * License (GNU Lesser General Public License) :
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 3 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Network.h"
#include "StableTime.h"
#include <esp_event_loop.h>
#include <apps/sntp/sntp.h>
#include <esp_log.h>

const char *st_tag = "StableTime";

StableTime *stableTime;

StableTime::StableTime() {

}

StableTime::StableTime(const char *dst) {
  if (dst != 0)
    setenv("TZ", dst, 1);
}

StableTime::~StableTime() {
}

void StableTime::loop(struct timeval *ptv) {
  now = *ptv;
}

void StableTime::loop(time_t n) {
  now.tv_sec = n;
  now.tv_usec = 0;
}

time_t StableTime::loop() {
  gettimeofday(&now, 0);
  return now.tv_sec;
}

time_t StableTime::Query() {
  return now.tv_sec;
}

void StableTime::Query(struct timeval *ptv) {
  *ptv = now;
}

// Returns pointer to static memory area
char *StableTime::TimeStamp(time_t t) {
  struct tm *tmp = localtime(&t);
  strftime(ts, sizeof(ts), "%Y-%m-%d %T", tmp);
  return ts;
}

// Returns pointer to static memory area
char *StableTime::TimeStamp() {
  struct tm *tmp = localtime(&now.tv_sec);
  strftime(ts, sizeof(ts), "%Y-%m-%d %T", tmp);
  return ts;
}
