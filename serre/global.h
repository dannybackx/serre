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
#include "esptime.h"

extern double		newPressure, newTemperature, oldPressure, oldTemperature;
extern int		valve;
extern time_t		tsboot;
extern int		nreconnects;
extern struct tm	*now;
extern char		SystemInfo1[], SystemInfo2[];

/*
 *
 */

extern int personal_timezone;
extern const char *mqtt_server;
extern const int mqtt_port;
extern char *schedule_string;
extern int report_frequency_seconds;

extern const char *mqtt_topic;
extern const char *mqtt_topic_bmp180;
extern const char *mqtt_topic_valve;
extern const char *mqtt_topic_valve_start;
extern const char *mqtt_topic_valve_stop;
extern const char *mqtt_topic_report;
extern const char *mqtt_topic_report_freq;
extern const char *mqtt_topic_reconnects;
extern const char *mqtt_topic_restart;
extern const char *mqtt_topic_boot_time;
extern const char *mqtt_topic_current_time;
extern const char *mqtt_topic_schedule;
extern const char *mqtt_topic_schedule_set;
extern const char *mqtt_topic_verbose;
extern const char *mqtt_topic_version;
extern const char *mqtt_topic_info;


// Pin definitions
extern const int	valve_pin;

extern const char *startup_text;

extern int verbose;
extern char *watering_schedule_string;

#define	VERBOSE_OTA		0x01
#define	VERBOSE_VALVE		0x02
#define	VERBOSE_BMP		0x04
#define	VERBOSE_CALLBACK	0x08
#define	VERBOSE_SYSTEM		0x10
#define	VERBOSE_WATER		0x20
#define	VERBOSE_DS3231		0x40
#define	VERBOSE_8		0x80


// We only need this in C++ files

#ifdef __cplusplus
#include <ESP8266WiFi.h>
#include "SFE_BMP180.h"
#ifdef SERRE
#include "Water.h"
extern Water *water;
#else
#include "Hatch.h"
extern Hatch *hatch;
#endif
#include "Ifttt.h"

extern SFE_BMP180	*bmp;
extern int BMPQuery();
#endif

void ValveReset(), ValveOpen();
void SetState(int);
extern int mywifi();

extern char *ntp_server_1, *ntp_server_2;
extern time_t		tsnow, tsboot, tsprevious;
extern esptime		*et;
extern int		mqtt_initial;
extern char		buffer[64];
