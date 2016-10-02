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

char		progmem_bfr[32];
// Length limit (31 chars)      		"..............................."

const char timedate_fmt[] PROGMEM =		"%02d:%02d:%02d %02d/%02d/%04d";
const char sensor_up_string[] PROGMEM =		"Sensor UP";
const char sensor_down_string[] PROGMEM =	"Sensor DOWN";
const char button_up_string[] PROGMEM =		"Button UP";
const char button_down_string[] PROGMEM =	"Button Down";
const char bmp_fmt[] PROGMEM =			"bmp (%2d.%02d, %d)";
const char no_sensor_string[] PROGMEM =		"No sensor detected";
const char setting_rtc[] PROGMEM =		"Setting RTC ";
const char initializing_bmp180[] PROGMEM =	"Initializing BMP180 ... ";
const char starting_wifi[] PROGMEM =		"\nStarting WiFi ...";
const char startup_text1[] PROGMEM =		"\nArduino Uno Wifi Chicken Hatch";
const char startup_text2[] PROGMEM =		"(c) 2016 by Danny Backx\n";
const char unmatched_command[] PROGMEM =	"Unmatched command {";


const char rcmd_time_set[] PROGMEM =		"/arduino/digital/time/set/";
const char rcmd_timezone_set[] PROGMEM =	"/arduino/digital/timezone/set/";

const char *gpm(const char *p)
{
  strcpy_P(progmem_bfr, p);
  return &progmem_bfr[0];
}

