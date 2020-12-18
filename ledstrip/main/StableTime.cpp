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

esp_err_t StNetworkConnected(void *ctx, system_event_t *event);
esp_err_t StNetworkDisconnected(void *ctx, system_event_t *event);

StableTime *stableTime;

StableTime::StableTime() {
  // stableTime = this;		// Already happens in Keypad.cpp

  network->RegisterModule(st_tag, StNetworkConnected, StNetworkDisconnected);
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

char *StableTime::TimeStamp() {
  struct tm *tmp = localtime(&now.tv_sec);
  strftime(ts, sizeof(ts), "%Y-%m-%d %T", tmp);
  return ts;
}

static void sntp_sync_notify(struct timeval *tvp) {
  char ts[20];
  struct tm *tmp = localtime(&tvp->tv_sec);
  strftime(ts, sizeof(ts), "%Y-%m-%d %T", tmp);
  ESP_LOGE(st_tag, "Timesync event %s", ts);
}

esp_err_t StNetworkConnected(void *ctx, system_event_t *event) {
  ESP_LOGI(st_tag, "NetworkConnected : start SNTP");

  sntp_setoperatingmode(SNTP_OPMODE_POLL);

  sntp_init();

#ifdef	NTP_SERVER_0
  sntp_setservername(0, (char *)NTP_SERVER_0);
#endif
#ifdef	NTP_SERVER_1
  sntp_setservername(1, (char *)NTP_SERVER_1);
#endif
  sntp_setservername(2, (char *)"3.ubuntu.pool.ntp.org");	// fallback

  sntp_set_time_sync_notification_cb(sntp_sync_notify);
  return ESP_OK;
}

esp_err_t StNetworkDisconnected(void *ctx, system_event_t *event) {
  ESP_LOGE(st_tag, "Network disconnect ...");
  return ESP_OK;
}
