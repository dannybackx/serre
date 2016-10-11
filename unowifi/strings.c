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
 *
 * Initialization, and code to cope with strings in program memory
 */
#include <avr/pgmspace.h>

char		progmem_bfr[57];

// Length limit (56 chars)      		"........................................................"
const char timedate_fmt[] PROGMEM =		"%02d:%02d:%02d %02d/%02d/%04d";
const char sensor_up_string[] PROGMEM =		"Sensor UP";
const char sensor_down_string[] PROGMEM =	"Sensor DOWN";
const char button_up_string[] PROGMEM =		"Button UP";
const char button_down_string[] PROGMEM =	"Button DOWN";
const char light_sensor_string[] PROGMEM =	"Light sensor";
const char bmp_fmt[] PROGMEM =			"bmp (%2d.%02d, %d)";
const char no_sensor_string[] PROGMEM =		"No sensor detected";
const char setting_rtc[] PROGMEM =		"Setting RTC ";
const char initializing_bmp180[] PROGMEM =	"Initializing BMP180 ... ";
const char starting_wifi[] PROGMEM =		"\nStarting WiFi ...";
const char startup_text1[] PROGMEM =		"\nArduino Uno Wifi Chicken Hatch";
const char startup_text2[] PROGMEM =		"(c) 2016 by Danny Backx\n";
const char unmatched_command[] PROGMEM =	"Unmatched command {";

const char time_query[] PROGMEM =		"/arduino/digital/time/query";
const char boot_time_query[] PROGMEM =		"/arduino/digital/boot/query";
const char rcmd_time_set[] PROGMEM =		"/arduino/digital/time/set/";
const char rcmd_timezone_set[] PROGMEM =	"/arduino/digital/timezone/set/";

const char query_bmp180[] PROGMEM =		"/arduino/digital/bmp180";
const char hatch_query[] PROGMEM =		"/arduino/digital/hatch/query";
const char hatch_up[] PROGMEM =			"/arduino/digital/hatch/up";
const char hatch_down[] PROGMEM =		"/arduino/digital/hatch/down";
const char hatch_stop[] PROGMEM =		"/arduino/digital/hatch/stop";
const char hatch_use_motor[] PROGMEM =		"Hatch : use motor ";
const char hatch_state_fmt[] PROGMEM =		"Hatch state %d";

const char set_schedule_to[] PROGMEM =		"Schedule set to : ";
const char schedule_query[] PROGMEM =		"/arduino/digital/schedule/query";
const char schedule_update[] PROGMEM =		"/arduino/digital/schedule/set/";
const char schedule[] PROGMEM =			"Schedule : ";

const char light_sensor_query[] PROGMEM =	"/arduino/digital/light/query";

const char version_query[] PROGMEM =		"/arduino/digital/version/query";
const char no_info[] PROGMEM =			"No version info available";
const char server_build[] PROGMEM =		"Server build ";
const char rtc_failure[] PROGMEM =		"Unable to sync with the RTC";

const char answer_ok[] PROGMEM =		"Ok";
const char ready[] PROGMEM =			"Ready";
const char boot[] PROGMEM =			"Boot";

const char maxtime_query[] PROGMEM =		"/arduino/digital/maxtime/query";
const char maxtime_set[] PROGMEM =		"/arduino/digital/maxtime/set/";

const char ts_url[] PROGMEM =			"api.thingspeak.com";

const char ts_1[] PROGMEM =			"/update?api_key=%s&field1=%d.%02d";
const char ts_2[] PROGMEM =			"/update?api_key=%s&field2=%d";
const char ts_3[] PROGMEM =			"/update?api_key=%s&field4=%d";
const char ts_4[] PROGMEM =			"/update?api_key=%s&field3=%d";
const char ts_123[] PROGMEM =			"/update?api_key=%s&field1=%d.%02d&field2=%d&field4=%d";
const char ts_timeout[] PROGMEM =		"TS timeout ";
const char ts_get_fail[] PROGMEM =		"TS GET fail ";
const char ts_feed[] PROGMEM =			"ThingSpeak feed at ";
const char ts_state_change[] PROGMEM =		"ThingSpeak state change to ";
const char ts_colon[] PROGMEM =			"ThingSpeak : ";

const char out_of_memory[] PROGMEM =		"Out of memory";

// Length limit (56 chars)      		"........................................................"

const char *gpm(const char *p)
{
  strcpy_P(progmem_bfr, p);
  return &progmem_bfr[0];
}

