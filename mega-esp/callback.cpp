/*
 * Copyright (c) 2016, 2017 Danny Backx
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

#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>

#include <Wire.h>
#include "SFE_BMP180.h"
#include "TimeLib.h"
#include "Hatch.h"
#include "Light.h"
#include "ThingSpeak.h"
#include "Sunset.h"
#include "global.h"
#include <TimeLib.h>
#include <DS1307RTC.h>

#ifdef BUILT_BY_MAKE
#include "buildinfo.h"
#endif

char reply[20];

typedef void (*handler)(char *, char *);

void ProcessCallback(char *, char *);
void BMP180Query(char *topic, char *message);
void DateTimeSet(char *topic, char *message);
void DateTimeQuery(char *topic, char *message);
void BootTimeQuery(char *topic, char *message);
void TimezoneSet(char *topic, char *message);
void MaxTimeSet(char *topic, char *message);
void MaxTimeQuery(char *topic, char *message);
void HatchQuery(char *topic, char *message);
void HatchUp(char *topic, char *message);
void HatchDown(char *topic, char *message);
void HatchStop(char *topic, char *message);
void LightDurationSet(char *topic, char *message);
void LightDurationQuery(char *topic, char *message);
void ScheduleSet(char *topic, char *message);
void ScheduleQuery(char *topic, char *message);
void VersionQuery(char *topic, char *message);
void LightSensorQuery(char *topic, char *message);
// void SensorQuery(char *topic, char *message);
void TestMotor(char *topic, char *message);

void ButtonQuery(char *topic, char *message);
void ButtonsQuery(char *topic, char *message);
void SensorQuery(char *topic, char *message);
void SensorsQuery(char *topic, char *message);
void SunsetReset(char *topic, char *message);
void SunsetGetSchedule(char *topic, char *message);

void DebugLoop(char *topic, char *message);

int ix;
struct mqtt_callback_table {
  char		*token;
  handler	f;
  int		len;
} mqtt_callback_table [] = {
  { "/BMP180/query",		BMP180Query,		0},
  { "/timezone/set/",		TimezoneSet,		0},
  { "/hatch/query",		HatchQuery,		0},
  { "/hatch/up",		HatchUp,		0},
  { "/hatch/down",		HatchDown,		0},
  { "/hatch/stop",		HatchStop,		0},
  { "/schedule/set/",		ScheduleSet,		0},
  { "/schedule/query",		ScheduleQuery,		0},
  { "/buttons/query",		ButtonsQuery,		0},
  { "/button/query/",		ButtonQuery,		0},
  { "/sensors/query",		SensorsQuery,		0},
  { "/sensor/query",		SensorQuery,		0},
  { "/sunset/reset",		SunsetReset,		0},
  { "/sunset/query",		SunsetGetSchedule,	0},
  { "/debug/loop",		DebugLoop,		0},
  { "/date/query",		DateTimeQuery,		0},
  { "/date/set/",		DateTimeSet,		0},
  { "/boot/query",		BootTimeQuery,		0},
  { "/maxtime/query",		MaxTimeQuery,		0},
  { "/maxtime/set/",		MaxTimeSet,		0},
  { "/light/duration/set/",	LightDurationSet,	0},
  { "/light/duration/query",	LightDurationQuery,	0},
  { "/light/query",		LightSensorQuery,	0},
  { "/analog/query",		LightSensorQuery,	0},
  // { "/sensor/query",		SensorQuery,		0},
  { "/version/query",		VersionQuery,		0},
  // { "/esp-link/kippen/1",	ProcessCallback,	0},	// test
  { "/motor/set/",		TestMotor,	0},	// test
  { NULL, NULL}
};

/*
 * Create the lengths in the table
 * Only subscribe to one topic : "kippen". Everything else is in the message.
 */
void mqConnected(void *response) {
  // Serial.println("MQTT connect");

  for (int i=0; mqtt_callback_table[i].token != NULL; i++) {
    if (mqtt_callback_table[i].len == 0)
      mqtt_callback_table[i].len = strlen(mqtt_callback_table[i].token);

    // mqtt.subscribe(mqtt_callback_table[i].token);
  }

  mqtt.subscribe("kippen");
}

void mqDisconnected(void *response) {
  // Serial.println("MQTT disconnect");
}

void mqData(void *response) {
  ELClientResponse *res = (ELClientResponse *)response;
  String topic = res->popString();
  String data = res->popString();

  // Debug("MQTT topic {%s} message {%s}", topic.c_str(), data.c_str());

  // Shouldn't be necessary, we're only subscribed to this.
  if (strncasecmp(topic.c_str(), "kippen", 6) != 0)
    return;

  for (ix=0; mqtt_callback_table[ix].token; ix++)
    if (strncasecmp(data.c_str(), mqtt_callback_table[ix].token,
          mqtt_callback_table[ix].len) == 0) {
      mqtt_callback_table[ix].f((char *)topic.c_str(), (char *)data.c_str());
    }
}

static int busy = 1;
#define IP(ip, x) ((int)((ip >> (8*x)) & 0xFF))

// Unlock this, called from setup().
void StartTrackStatus() {
  busy = 0;
}

// This can be used to detect connectivity changes
void WifiStatusCallback(void *response) {
  ELClientResponse *res = (ELClientResponse *)response;

  if (busy == 1)
    return;
  busy = 1;

  if (res->argc() == 1) {
    uint8_t status;
    res->popArg(&status, 1);

    if (status == STATION_GOT_IP) {
#if 0
      Serial.println("Network status change\n");
#else
      // Share our IP address
      uint32_t ip, nm, gw;
      cmd.GetWifiInfo(&ip, &nm, &gw);

      sprintf(buffer, "Network status change, IP %d.%d.%d.%d",
        IP(ip, 0), IP(ip, 1), IP(ip, 2), IP(ip, 3));
      Serial.println(buffer);
#endif
    }
  }

  busy = 0;
}

void BMP180Query(char *topic, char *message) {
  if (bmp == 0)
    BMPInitialize();

  if (bmp) {
    BMPQuery();

    int a, b, c;
    // Temperature
    a = (int) newTemperature;
    double td = newTemperature - a;
    b = 100 * td;
    c = newPressure;

    // Format the result
    sprintf(reply, "bmp (%2d.%02d, %d)", a, b, c);
 
  } else {
    sprintf(reply, "No sensor detected");
  }

  Serial.print("BMP180: "); Serial.println(reply);

  mqtt.publish(mqtt_topic, reply);
}

void DateTimeSet(char *topic, char *message) {
    //
    // Get input for this from date +T%s
    //
    // We're setting the RTC to local time here by subtracting personal_timezone
    // so the timezone related bugs are worked around.
    //
    const char *p = message,
         *q = p + mqtt_callback_table[ix].len;
    if (q[0] == 'T') {
      long t = atol(q+1);
      t += 3600 * personal_timezone;	// This is it
      RTC.set(t);
      Serial.print("Setting RTC ");
      Serial.print(q);
      Serial.print(" ");
      Serial.print(t);
      Serial.print(" ");
      sprintf(buffer, "%02d:%02d:%02d %02d/%02d/%04d",
        hour(), minute(), second(), day(), month(), year());
      Serial.println(buffer);
      mqtt.publish(mqtt_topic, "Date ok");
    }
}

void DateTimeQuery(char *topic, char *message) {
  sprintf(buffer, "Date %02d:%02d:%02d %02d/%02d/%04d",
    hour(), minute(), second(), day(), month(), year());
  mqtt.publish(mqtt_topic, buffer);
}

void BootTimeQuery(char *topic, char *message) {
  sprintf(buffer, "Boot %02d:%02d:%02d %02d/%02d/%04d",
    hour(boot_time), minute(boot_time), second(boot_time),
    day(boot_time), month(boot_time), year(boot_time));
  mqtt.publish(mqtt_topic, buffer);
}

void TimezoneSet(char *topic, char *message) {
  const char	*p = message,
		*q = p + mqtt_callback_table[ix].len;
    int t = atoi(q);
    personal_timezone = t;
    mqtt.publish(mqtt_topic, "Timezone ok");
}

void MaxTimeSet(char *topic, char *message) {
    const char	*p = message,
		*q = p + mqtt_callback_table[ix].len;
    hatch->setMaxTime(atoi(q));
    mqtt.publish(mqtt_topic, "Maxtime ok");
}

void MaxTimeQuery(char *topic, char *message) {
    sprintf(buffer, "Maxtime %d", hatch->getMaxTime());
    mqtt.publish(mqtt_topic, buffer);
}

/********************************************************************************
 *                                                                              *
 * Hatch                                                                        *
 *                                                                              *
 ********************************************************************************/
void HatchQuery(char *topic, char *message) {
    // sprintf(reply, "Hatch state %d", hatch->moving());
    sprintf(reply, "hatch motion %d position %d", hatch->moving(), hatch->getPosition());
    mqtt.publish(mqtt_topic, reply);
}

void HatchUp(char *topic, char *message) {
    hatch->Up(hour(nowts), minute(nowts), second(nowts));
    mqtt.publish(mqtt_topic, "Hatch ok");
}

void HatchDown(char *topic, char *message) {
    hatch->Down(hour(nowts), minute(nowts), second(nowts));
    mqtt.publish(mqtt_topic, "Hatch ok");
}

void HatchStop(char *topic, char *message) {
    time_t t = now();
    hatch->Stop(hour(t), minute(t), second(t));
    mqtt.publish(mqtt_topic, "Hatch ok");
}

/********************************************************************************
 *                                                                              *
 * Light                                                                        *
 *                                                                              *
 ********************************************************************************/
void LightDurationSet(char *topic, char *message) {
    const char	*p = message,
		*q = p + mqtt_callback_table[ix].len;
    light->setDuration(atoi(q));
    mqtt.publish(mqtt_topic, "Light ok");
}

void LightDurationQuery(char *topic, char *message) {
    sprintf(buffer, "Light %d", light->getDuration());
    mqtt.publish(mqtt_topic, buffer);
}

/********************************************************************************
 *                                                                              *
 * Buttons                                                                      *
 *                                                                              *
 ********************************************************************************/
void ButtonsQuery(char *topic, char *message) {
    sprintf(buffer, "Button up %d down %d", button_up, button_down);
    mqtt.publish(mqtt_topic, buffer);
}

void ButtonQuery(char *topic, char *message) {
    sprintf(buffer, "Button %d", light->getDuration());
    mqtt.publish(mqtt_topic, buffer);
}

/********************************************************************************
 *                                                                              *
 * Magnetic Sensors                                                             *
 *                                                                              *
 ********************************************************************************/
void SensorsQuery(char *topic, char *message) {
    sprintf(buffer, "Sensors up %d down %d", sensor_up, sensor_down);
    mqtt.publish(mqtt_topic, buffer);
}

void SensorQuery(char *topic, char *message) {
    sprintf(buffer, "Sensors %d", light->getDuration());
    mqtt.publish(mqtt_topic, buffer);
}

/********************************************************************************
 *                                                                              *
 * Schedule                                                                     *
 *                                                                              *
 ********************************************************************************/
void ScheduleSet(char *topic, char *message) {
    const char *p = message,
         *q = p + mqtt_callback_table[ix].len;
    hatch->setSchedule(q);
    // Serial.print("Schedule set to : ");
    // Serial.println(q);
    mqtt.publish(mqtt_topic, "Schedule ok");
}

void ScheduleQuery(char *topic, char *message) {
    // Serial.print("Schedule : ");
    char *sched = hatch->getSchedule();
    mqtt.publish(mqtt_topic, sched);
    // Serial.println(sched);
    free(sched);
}

void VersionQuery(char *topic, char *message) {
#ifdef BUILT_BY_MAKE
  char *s = (char *)malloc(100);
  sprintf(s, "%s%s%s %s",
    "Server build ",
    _BuildInfo.src_filename,
    _BuildInfo.date,
    _BuildInfo.time);
  mqtt.publish(mqtt_topic, s);
  Serial.println(s);
  free(s);
#else
  mqtt.publish(mqtt_topic, "No version info available");
#endif
}

/********************************************************************************
 *                                                                              *
 * Light sensor                                                                 *
 *                                                                              *
 ********************************************************************************/
void LightSensorQuery(char *topic, char *message) {
    sprintf(reply, "Light %d", light->query());
    mqtt.publish(mqtt_topic, reply);
    Serial.println(reply);
}

void ProcessCallback(char *topic, char *message) {
  Serial.print("MQTT ProcessCallback(");
  Serial.print(topic);
  Serial.print(") message (");
  Serial.print(message);
  Serial.println(")");
}

#include "SimpleL298.h"

SimpleL298 *motor = 0;

void TestMotor(char *topic, char *message) {
  Serial.print("MQTT TestMotor(");
  Serial.print(topic);
  Serial.print(") message (");
  Serial.print(message);
  Serial.println(")");

  const char *p = message,
	     *q = p + mqtt_callback_table[ix].len;
  int ms = atoi(q);
  Serial.print("Set motor speed to "); Serial.println(ms);

  if (motor == 0)
    motor = new SimpleL298(2, 3, 9);
  if (ms < 0) {
    motor->setSpeed(-ms);
    motor->run(BACKWARD);
  } else if (ms > 0) {
    motor->setSpeed(ms);
    motor->run(FORWARD);
  } else {
    motor->run(RELEASE);
    motor->setSpeed(0);
  }
}

/********************************************************************************
 *                                                                              *
 * Sunset                                                                       *
 *                                                                              *
 ********************************************************************************/
void SunsetReset(char *topic, char *message) {
  sunset->reset();
}

void SunsetGetSchedule(char *topic, char *message) {
  char buffer[80];
  sunset->getSchedule(buffer, sizeof(buffer));
  mqtt.publish(mqtt_topic, buffer);
}

/********************************************************************************
 *                                                                              *
 * Sunset                                                                       *
 *                                                                              *
 ********************************************************************************/
void DebugLoop(char *topic, char *message) {
  time_t nowts = now();
  
  // Query Sunset, feed value into Light
  enum lightState sun = sunset->loop(nowts);

  // Has the light changed ?
  enum lightState newlight = light->loop(nowts, sun);

  char buffer[80];
  sprintf(buffer, "loop sun %s light %s", light->Light2String(sun), light->Light2String(newlight));
  mqtt.publish(mqtt_topic, buffer);
}
