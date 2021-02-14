#define	DO_OTA
#define	DO_AHT
#define	DO_WEBSERVER

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

void doOTA();

#include "secrets.h"
#include "measure.h"
#include "buildinfo.h"
#include "ws.h"

#ifdef	DO_AHT
#include "aht10.h"
#endif

struct mywifi {
  const char *ssid, *pass;
} mywifi[] = {
#ifdef MY_SSID_1
  { MY_SSID_1, MY_WIFI_PASSWORD_1 },
#endif
#ifdef MY_SSID_2
  { MY_SSID_2, MY_WIFI_PASSWORD_2 },
#endif
#ifdef MY_SSID_3
  { MY_SSID_3, MY_WIFI_PASSWORD_3 },
#endif
#ifdef MY_SSID_4
  { MY_SSID_4, MY_WIFI_PASSWORD_4 },
#endif
#ifdef MY_SSID_5
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

// Arduino setup function
void setup() {
  Serial.begin(115200);
  Serial.printf("Starting %s (built %s %s on v%s)", app_name, _BuildInfo.date, _BuildInfo.time, _BuildInfo.env_version);

  // Try to connect to WiFi
  WiFi.mode(WIFI_STA);

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      Serial.printf("\nTrying (%d %d) %s .. ", ix, wifi_tries, mywifi[ix].ssid);
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
    }
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    Serial.printf("not connected, restarting .. ");
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  gws = gw.toString();

  Serial.printf("connected, ip %s, gw %s\n", ips.c_str(), gws.c_str());

  configTime(TZ_Europe_Brussels, "ntp.telenet.be");
  time_t local, utc;
  localtime(&local);
  gmtime(&utc);

  settimeofday_cb(time_is_set);

  ws_begin();

#ifdef	DO_OTA
  doOTA();
#endif

#ifdef	DO_AHT
  aht10_begin();
#endif
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

#ifdef DO_OTA
  ArduinoOTA.handle();
#endif

  delay(1);

#ifdef DO_AHT
  aht10_loop(the_time);
#endif

  ws_loop();
}

void time_is_set(bool from_sntp) {
  time_t now = time(0);
  struct tm *tmp = localtime(&now);

  Serial.printf("settimeofday(%s), time %04d.%02d.%02d %02d:%02d:%02d\n",
    from_sntp ? "SNTP" : "USER",
    tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

void doOTA() {
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
  sprintf(ret, "%04d.%02d.%02d %02d:%02d:%02d",
	tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
  return &ret[0];
}
