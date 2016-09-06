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
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "mywifi.h"

struct wifi {
  char *ssid, *password;
} APs[] = {
#if defined(MY_SSID) && defined(MY_WIFI_PASSWORD)
  { (char *)MY_SSID, (char *)MY_WIFI_PASSWORD },
#endif
#if defined(MY_SSID_1) && defined(MY_WIFI_PASS_1)
  { (char *)MY_SSID_1, (char *)MY_WIFI_PASS_1 },
#endif
#if defined(MY_SSID_2) && defined(MY_WIFI_PASS_2)
  { (char *)MY_SSID_2, (char *)MY_WIFI_PASS_2 },
#endif
#if defined(MY_SSID_3) && defined(MY_WIFI_PASS_3)
  { (char *)MY_SSID_3, (char *)MY_WIFI_PASS_3 },
#endif
#if defined(MY_SSID_4) && defined(MY_WIFI_PASS_4)
  { (char *)MY_SSID_4, (char *)MY_WIFI_PASS_4 },
#endif
#if defined(MY_SSID_5) && defined(MY_WIFI_PASS_5)
  { (char *)MY_SSID_5, (char *)MY_WIFI_PASS_5 },
#endif
  { (char *)NULL, (char *)NULL }
};

int mywifi() {
  int wifi_tries = 3;
  int wcr;

  while (wifi_tries-- >= 0) {
    for (int i=0; APs[i].ssid != NULL; i++) {
      WiFi.begin(APs[i].ssid, APs[i].password);
      Serial.print(".");
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        return wcr;
    }
  }

  return wcr;
}
