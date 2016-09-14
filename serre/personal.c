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
// #include "global.h"
#include "mywifi.h"

const int personal_timezone = +2;

const char *mqtt_server = "192.168.1.185";
const int mqtt_port = 1883;

const char *mqtt_topic =		SystemId;
const char *mqtt_topic_bmp180 =		SystemId "/BMP180";
const char *mqtt_topic_valve =		SystemId "/Valve";
const char *mqtt_topic_valve_start =	SystemId "/Valve/1";
const char *mqtt_topic_valve_stop =	SystemId "/Valve/0";
const char *mqtt_topic_report =		SystemId "/Report";
const char *mqtt_topic_report_freq =	SystemId "/Report/Frequency";
const char *mqtt_topic_restart =	SystemId "/System/Reboot";
const char *mqtt_topic_reconnects =	SystemId "/System/Reconnects";
const char *mqtt_topic_boot_time =	SystemId "/System/Boot";
const char *mqtt_topic_current_time =	SystemId "/System/Time";
const char *mqtt_topic_schedule_set =	SystemId "/Schedule/Set";
const char *mqtt_topic_schedule =	SystemId "/Schedule";
const char *mqtt_topic_verbose =	SystemId "/System/Verbose";
const char *mqtt_topic_version =	SystemId "/System/Version";
const char *mqtt_topic_info =		SystemId "/System/Info";

// Pin definitions
const int	valve_pin = 0;

const char *startup_text = "\n\nMQTT Greenhouse Watering and Sensor Module\n"
	"(c) 2016 by Danny Backx\n\n";

int verbose = 0;

/*
 * Schedule for watering
 *	format is CSV
 *	hour,onoff
 */
char *schedule_string = "21:20,1,21:30,0";

/*
 * NTP servers
 */
char	*ntp_server_1 = "ntp.scarlet.be",
	*ntp_server_2 = "ntp.belnet.be";

/*
 * Periodic report to ThingSpeak about sensor info
 * You'll need to define some values in mywifi.h for this to work.
 */
const int report_frequency_seconds = 900;
