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
 *
 * This contains code to query the API server at api.sunrise-sunset.org .
 * Notes :
 * - This needs a patch to esp-link/el-client to cope with reply length ~500 bytes.
 * - All times in GMT so timezone and DST must still be applied.
 *   This is now treated very simply by adding a hardcoded value right before
 *   storing the times in private variables.
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
#include "Kippen.h"
#include "Sunset.h"

Sunset::Sunset() {
  last_call = 0;
  stable = LIGHT_NONE;
}

Sunset::~Sunset() {
}

const char *ss_template = "http://api.sunrise-sunset.org/json?lat=%s&lng=%s&formatted=0";

void Sunset::query(const char *lat, const char *lon, char *msg) {
  char *query = (char *)malloc(strlen(ss_template) + strlen(lat) + strlen(lon));
  sprintf(query, ss_template, lat, lon);

  http_config.url = query;
  http_client = esp_http_client_init(&http_config);

  ESP_LOGD(sunset_tag, "Querying .. {%s}", query);

  esp_err_t err = esp_http_client_open(http_client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(sunset_tag, "HTTP GET request failed: %s, trying again after %d minutes",
      esp_err_to_name(err), error_delay / 60);

    esp_http_client_cleanup(http_client);
    the_delay = error_delay;
    free(query); query = 0; http_config.url = 0;

    return;
  }

  ESP_LOGD(sunset_tag, "Ok : got reply");

  int content_length = esp_http_client_fetch_headers(http_client);
  if (content_length < 0) {
    ESP_LOGE(sunset_tag, "Invalid content length %d, discarding", content_length);

    free(query); query = 0; http_config.url = 0;
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
    return;
  } else
    ESP_LOGD(sunset_tag, "Content length %d", content_length);

  if (content_length == 0)
    content_length = buflen;	// Allocate too much, but not for a very long time

  buf = (char *)malloc(content_length + 1);
  if (buf == 0) {
    ESP_LOGE(sunset_tag, "malloc failed");
    free(query); query = 0; http_config.url = 0;
    return;
  }

  int pos = 0, total = 0, rlen = 0;
  while (total < content_length && err == ESP_OK) {
    rlen = esp_http_client_read(http_client, buf + pos, content_length - total);
    if (rlen < 0) {
      ESP_LOGE(sunset_tag, "error reading data");
      esp_http_client_close(http_client);
      esp_http_client_cleanup(http_client);
      free(buf); buf = 0;
      free(query); query = 0; http_config.url = 0;
      return;
    }
    if (rlen == 0)
      break;
    buf[rlen] = 0;
    // ESP_LOGI(sunset_tag, "rlen = %d", rlen);

    pos += rlen;
    total += rlen;
  }

  ESP_LOGD(sunset_tag, "Query %s", query);
  ESP_LOGD(sunset_tag, "Response %s", buf);

  if (last_error) {
    last_error = false;
    ESP_LOGE(sunset_tag, "Last_error set, response %s", buf);
  }

  free(query); query = 0; http_config.url = 0;

  esp_http_client_close(http_client);
  esp_http_client_cleanup(http_client);

  boolean fail = true;

  DynamicJsonBuffer jb;
  JsonObject &root = jb.parseObject(buf);
  if (root.success()) {
    // ESP_LOGD(sunset_tag, "Root ok");

    const char *s_sunrise = root["results"]["sunrise"];
    const char *s_sunset = root["results"]["sunset"];
    const char *s_twilight_begin = root["results"]["civil_twilight_begin"];
    const char *s_twilight_end = root["results"]["civil_twilight_end"];

    // "sunrise":"2020-08-13T04:58:47+00:00" --> 04 * 100 + 58 --> 0498
    sunrise = TimeOnly(s_sunrise);
    sunset = TimeOnly(s_sunset);
    twilight_begin = TimeOnly(s_twilight_begin);
    twilight_end = TimeOnly(s_twilight_end);

    ESP_LOGI(sunset_tag, "Decoded sunrise %04d sunset %04d twilight begin %04d end %04d",
      sunrise, sunset, twilight_begin, twilight_end);

    fail = false;
  }

  if (fail) {
    last_error = true;

    ESP_LOGE(sunset_tag, "Failed to parse JSON. Response length %d, {%s}", content_length, buf);

    the_delay = error_delay;				// Shorter retry
    free(buf); buf = 0;
    free(query); query = 0;
    return;
  }

  free(buf);
  buf = 0;
  free(query); query = 0;

  the_delay = normal_delay;
}

/*
 * Pick just hour and minute, and translate into (not quite) minutes since midnight (easily comparable)
 * We multiply by 100 instead of 60 so it's more readable; it's still as comparable...
 */
int Sunset::TimeOnly(const char *s) {
  // Note this can be done as string parsing locally but I'm using library functions
  struct tm tm;
  strptime(s, "%Y-%m-%dT%H:%M:%S", &tm);
  return tm.tm_hour * 100 + tm.tm_min;
}

/*
 * Query once per day
 * All times in local timezone.
 */
enum lightState Sunset::loop(time_t now) {
  // First call ever, so we just got initialized.
  // Don't bother querying, this is done from elsewhere.
  if (last_call == 0) {
    last_call = now;
    return LIGHT_NONE;
  }

  struct tm *tmp = localtime(&now);

  /* Refresh our knowledge from time to time */
  if (now - last_call > delay_queries) {
    // Don't do this immediately after boot or midnight
    if (now < 1000 || tmp->tm_hour < 4)
      return LIGHT_NIGHT;

    char _today[32];
    sprintf(_today, "%04d-%02d-%02d, %02d:%02d", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday,
      tmp->tm_hour, tmp->tm_min);
    query(lat, lon, _today);
    last_call = now;
  }

  time_t tt = tmp->tm_hour * 3600L + tmp->tm_min * 60L + tmp->tm_sec;

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
  last_call = 0L;	// Causes re-query
  Serial.println("Sunset : reset");
}

/*
 * Produce a human readable form of the times we currently work with
 */
void Sunset::getSchedule(char *buffer, int bl) {
  char *p;

  if (bl < 34) {
    buffer[0] = 0;
    return;
  }

  strcpy(buffer, "sunrise ");
  p = buffer + strlen(buffer);

  strcpy(p, Time2String(sunrise));
  p = buffer + strlen(buffer);

  strcpy(p, " sunset ");
  p = buffer + strlen(buffer);

  strcpy(p, Time2String(sunset));
}
