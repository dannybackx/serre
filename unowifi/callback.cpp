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
#include <ArduinoWiFi.h>
#include <Wire.h>
#include "SFE_BMP180.h"
#include "Hatch.h"
#include "global.h"
#include <TimeLib.h>
#include <DS1307RTC.h>

#ifdef BUILT_BY_MAKE
#include "buildinfo.h"
#endif

char reply[20];

void ProcessCallback(WifiData client) {
  String command = client.readString();
 
  Serial.print(F("ProcessCallback {"));
  Serial.print(command);
  Serial.println(F("}"));

  /********************************************************************************
   *                                                                              *
   * Environmental sensor (BMP180)                                                *
   *                                                                              *
   ********************************************************************************/
  if (command == gpm(query_bmp180)) {
      if (bmp) {
        BMPQuery();

        int a, b, c;
        // Temperature
        a = (int) newTemperature;
        double td = newTemperature - a;
        b = 100 * td;
        c = newPressure;

        // Format the result
        sprintf(reply, gpm(bmp_fmt), a, b, c);
  
      } else {
        sprintf(reply, gpm(no_sensor_string));
      }
    client.print(reply);
  /********************************************************************************
   *                                                                              *
   * Manage time                                                                  *
   *                                                                              *
   ********************************************************************************/
  } else if (command.startsWith(gpm(rcmd_time_set))) {
    const char *p = command.c_str(),
         *q = p + strlen(progmem_bfr);
    if (q[0] == 'T') {
      long t = atol(q+1);
      RTC.set(t);
      Serial.print(gpm(setting_rtc));
      Serial.print(q);
      Serial.print(" ");
      Serial.print(t);
      Serial.print(" ");
      sprintf(buffer, gpm(timedate_fmt),
        hour() + personal_timezone, minute(), second(), day(), month(), year());
      Serial.println(buffer);
    }
  } else if (command.startsWith(gpm(rcmd_timezone_set))) {
    const char	*p = command.c_str(),
		*q = p + strlen(progmem_bfr);
    int t = atoi(q);
    personal_timezone = t;
  /********************************************************************************
   *                                                                              *
   * Hatch                                                                        *
   *                                                                              *
   ********************************************************************************/
  } else if (command == gpm(hatch_query)) {
    sprintf(reply, "Hatch state %d", hatch->moving());
    client.print(reply);
  } else if (command == gpm(hatch_up)) {
    hatch->Up();
  } else if (command == gpm(hatch_down)) {
    hatch->Down();
  } else if (command == gpm(hatch_stop)) {
    hatch->Stop();

  /********************************************************************************
   *                                                                              *
   * Schedule                                                                     *
   *                                                                              *
   ********************************************************************************/
  } else if (command == gpm(schedule_update)) {
    const char *p = command.c_str(),
         *q = p + strlen(progmem_bfr);
    hatch->setSchedule(q);
    Serial.print(gpm(set_schedule_to));
    Serial.println(q);
  } else if (command == gpm(schedule_query)) {
    char *sched = hatch->getSchedule();
    client.print(sched);
    Serial.print("Schedule : ");
    Serial.println(sched);
    free(sched);

  } else if (command == gpm(version_query)) {
#ifdef BUILT_BY_MAKE
    // client.print("Server build ");
    client.println(_BuildInfo.src_filename);
    delay(100);
    client.print(_BuildInfo.date);
    client.print(" ");
    client.println(_BuildInfo.time);

    Serial.print("Server build ");
    Serial.println(_BuildInfo.src_filename);
    Serial.print(_BuildInfo.date);
    Serial.print(" ");
    Serial.println(_BuildInfo.time);
#else
    client.print(gpm(no_info));
#endif
  /********************************************************************************
   *                                                                              *
   * Add more cases here                                                          *
   *                                                                              *
   ********************************************************************************/
  // } else if (command.startsWith("/arduino/digital/time/set/")) {
  // } else if (command == gpm("/arduino/digital/time/set/")) {

  } else {
    Serial.print(gpm(unmatched_command));
    Serial.print(command);
    Serial.println("}");
  }

  // This is needed to close the communication
  client.print(EOL);
}
