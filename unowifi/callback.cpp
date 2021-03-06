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
#include "TimeLib.h"
#include "Hatch.h"
#include "Light.h"
#include "ThingSpeak.h"
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
    //
    // Get input for this from date +T%s
    //
    // We're setting the RTC to local time here by subtracting personal_timezone
    // so the timezone related bugs are worked around.
    //
    const char *p = command.c_str(),
         *q = p + strlen(progmem_bfr);
    if (q[0] == 'T') {
      long t = atol(q+1);
      t += 3600 * personal_timezone;	// This is it
      RTC.set(t);
      Serial.print(gpm(setting_rtc));
      Serial.print(q);
      Serial.print(" ");
      Serial.print(t);
      Serial.print(" ");
      sprintf(buffer, gpm(timedate_fmt),
        hour(), minute(), second(), day(), month(), year());
      Serial.println(buffer);
      client.println(answer_ok);
    }
  } else if (command.startsWith(gpm(time_query))) {
      sprintf(buffer, gpm(timedate_fmt),
        hour(), minute(), second(), day(), month(), year());
      client.println(buffer);

  } else if (command.startsWith(gpm(boot_time_query))) {
      sprintf(buffer, gpm(timedate_fmt),
        hour(boot_time), minute(boot_time), second(boot_time),
	day(boot_time), month(boot_time), year(boot_time));
      client.println(buffer);

  } else if (command.startsWith(gpm(rcmd_timezone_set))) {
    const char	*p = command.c_str(),
		*q = p + strlen(progmem_bfr);
    int t = atoi(q);
    personal_timezone = t;
    client.println(answer_ok);

  } else if (command.startsWith(gpm(maxtime_set))) {
    const char	*p = command.c_str(),
		*q = p + strlen(progmem_bfr);
    hatch->setMaxTime(atoi(q));
    client.println(answer_ok);

  } else if (command == gpm(maxtime_query)) {
    sprintf(buffer, "%d", hatch->getMaxTime());
    client.println(buffer);

  /********************************************************************************
   *                                                                              *
   * Hatch                                                                        *
   *                                                                              *
   ********************************************************************************/
  } else if (command == gpm(hatch_query)) {
    sprintf(reply, gpm(hatch_state_fmt), hatch->moving());
    client.println(reply);
  } else if (command == gpm(hatch_up)) {
    hatch->Up();
    client.println(answer_ok);
  } else if (command == gpm(hatch_down)) {
    hatch->Down();
    client.println(answer_ok);
  } else if (command == gpm(hatch_stop)) {
    hatch->Stop();
    client.println(answer_ok);

  /********************************************************************************
   *                                                                              *
   * Light                                                                        *
   *                                                                              *
   ********************************************************************************/
  } else if (command.startsWith(gpm(light_duration_set))) {
    const char	*p = command.c_str(),
		*q = p + strlen(progmem_bfr);
    light->setDuration(atoi(q));
    client.println(answer_ok);

  } else if (command == gpm(light_duration_query)) {
    sprintf(buffer, "%d", light->getDuration());
    client.println(buffer);

  /********************************************************************************
   *                                                                              *
   * Schedule                                                                     *
   *                                                                              *
   ********************************************************************************/
  } else if (command.startsWith(gpm(schedule_update))) {
    const char *p = command.c_str(),
         *q = p + strlen(progmem_bfr);
    hatch->setSchedule(q);
    // Serial.print(gpm(set_schedule_to));
    // Serial.println(q);
    client.println(answer_ok);

  } else if (command == gpm(schedule_query)) {
    // Serial.print(gpm(schedule));
    char *sched = hatch->getSchedule();
    client.print(sched);
    // Serial.println(sched);
    free(sched);

  } else if (command == gpm(version_query)) {
#ifdef BUILT_BY_MAKE
    client.print(gpm(server_build));
    client.println(_BuildInfo.src_filename);
    client.print(_BuildInfo.date);
    client.print(" ");
    client.println(_BuildInfo.time);

    Serial.print(gpm(server_build));
    Serial.println(_BuildInfo.src_filename);
    Serial.print(_BuildInfo.date);
    Serial.print(" ");
    Serial.println(_BuildInfo.time);
#else
    client.print(gpm(no_info));
#endif
  /********************************************************************************
   *                                                                              *
   * Light sensor                                                                 *
   *                                                                              *
   ********************************************************************************/
  } else if (command == gpm(light_sensor_query)) {
    sprintf(reply, "Light %d", light->query());
    client.println(reply);
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
