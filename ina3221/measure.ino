#define	DO_AHT
/*
 * Measurement station, with web server
 *
 * Copyright (c) 2016, 2017, 2019, 2020, 2021 Danny Backx
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
#include <ArduinoOTA.h>

#include <coredecls.h>
#include <TZ.h>

void ConfigureOTA();

#include "secrets.h"
#include "measure.h"
#include "buildinfo.h"
#include "ws.h"
#include "Control.h"

#ifdef	DO_AHT
#include "aht10.h"
#endif
#include "ina3221.h"
#include "d1mini.h"
#include "ads1115.h"
#include <html.h>

struct mywifi {
  const char *ssid, *pass;
} mywifi[] = {
#if defined(MY_SSID_1) && defined(MY_WIFI_PASSWORD_1)
  { MY_SSID_1, MY_WIFI_PASSWORD_1 },
#endif
#if defined(MY_SSID_2) && defined(MY_WIFI_PASSWORD_2)
  { MY_SSID_2, MY_WIFI_PASSWORD_2 },
#endif
#if defined(MY_SSID_3) && defined(MY_WIFI_PASSWORD_3)
  { MY_SSID_3, MY_WIFI_PASSWORD_3 },
#endif
#if defined(MY_SSID_4) && defined(MY_WIFI_PASSWORD_4)
  { MY_SSID_4, MY_WIFI_PASSWORD_4 },
#endif
#if defined(MY_SSID_5) && defined(MY_WIFI_PASSWORD_5)
  { MY_SSID_5, MY_WIFI_PASSWORD_5 },
#endif
  { NULL, NULL}
};

WiFiClient	espClient;
const char *app_name = APP_NAME;

// Forward
void time_is_set(bool from_sntp);

// All kinds of variables
int		verbose = 0;
String		ips, gws;
int		state = 0;
int		manual = 0;
time_t		boot_time = 0;

// Arduino setup function
void setup() {
  Serial.begin(115200);
  Serial.printf("Starting %s (built %s %s on v%s)", app_name, _BuildInfo.date, _BuildInfo.time, _BuildInfo.env_version);

  // Try to connect to WiFi
  WiFi.mode(WIFI_STA);

  if (mywifi[0].ssid == 0) {
    Serial.printf("No WiFi configured, failing...\n");
    while (1)
      delay(10000);
    /*NOTREACHED*/
  }

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    Serial.printf("\nTrying %s ", mywifi[ix].ssid);
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      // Serial.printf("\nTrying (%d %d) %s .. ", ix, wifi_tries, mywifi[ix].ssid);
      Serial.printf(".");
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
    }
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    Serial.printf(" not connected, restarting .. ");
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  gws = gw.toString();

  Serial.printf(" connected, ip %s, gw %s\n", ips.c_str(), gws.c_str());

  configTime(TZ_Europe_Brussels, "ntp.telenet.be");
  time_t local, utc;
  localtime(&local);
  gmtime(&utc);

  settimeofday_cb(time_is_set);

  ConfigureOTA();

  // Need this before initializing sensors
  control = new Control();

#ifdef	DO_AHT
  aht10_begin();
#endif
  ina3221_begin();
  d1mini_begin();
  ads1115_begin();

  for (int a1 = MY_ALLOC1; a1 > 2; a1--) {
    if (control->setAllocation(a1 * MY_ALLOC2)) {
      Serial.printf("%s: allocation ok for %d items\n", __FUNCTION__, a1 * MY_ALLOC2);
      break;
    } else {
      Serial.printf("%s: failed to allocate %d items, falling back\n", __FUNCTION__, a1 * MY_ALLOC2);
    }
  }

  // Start web server after we've created all sensors
  ws_begin();
}

int	old_minute, old_hour;
time_t	the_time;
int	the_day, the_month, the_year, the_dow, the_hour, the_minute;

void loop() {
  // Get the time
  old_minute = the_minute;
  old_hour = the_hour;

  the_time = time(0);
  struct tm *tmp = localtime(&the_time);

  the_hour = tmp->tm_hour;
  the_minute = tmp->tm_min;

  the_day = tmp->tm_mday;
  the_month = tmp->tm_mon + 1;
  the_year = tmp->tm_year + 1900;
  the_dow = tmp->tm_wday;

  ArduinoOTA.handle();

  delay(1);

#ifdef DO_AHT
  aht10_loop(the_time);
#endif
  ina3221_loop(the_time);
  d1mini_loop(the_time);
  ads1115_loop(the_time);

  ws_loop();
}

void time_is_set(bool from_sntp) {
  time_t now = time(0);
  struct tm *tmp = localtime(&now);

  const char *fmt = time_msg_format;
  if (boot_time == 0) {
    boot_time = now;
    fmt = boot_msg_format;
  }

  Serial.printf(fmt, tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

void ConfigureOTA() {
  static int OTAprev;

  Serial.printf("Configuring OTA ... ");

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
    OTAprev = curr;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error\n");
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);

  ArduinoOTA.begin();

  Serial.printf("done\n");
}

char *timestamp(time_t t) {
  static char ret[80];
  tm *tmp = localtime(&t);
  sprintf(ret, timestamp_format,
	tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
  return &ret[0];
}
