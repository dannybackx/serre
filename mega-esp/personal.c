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

int personal_timezone = +1;

int verbose = 0;

/*
 * Schedule for hatch
 *	format is CSV
 *	hour,updown
 */
char *schedule_string = "21:00,-1,07:35,1";

/*
 * Periodic report to ThingSpeak about sensor info
 * You'll need to define some values in secrets.h for this to work.
 */
const int report_frequency_seconds = 900;

int light_treshold_high =	700,	// Depends on components used.
    light_treshold_low =	400,
    light_min_duration =	500;	// 5 minutes

const int	loop_delay =	50;

/*
 * Pin definitions
 *
 * If the definition is < 0 then assume it's not hooked up.
 */
// Sensors that indicate when the hatch is at the up or down position
const int	sensor_up_pin =		22;	// Digital
const int	sensor_down_pin =	23;	// Digital

const int	button_up_pin =		26;	// Digital
const int	button_down_pin =	27;	// Digital

const int	light_sensor_pin =	57;	// Analog, A3 translates to 57 on a Mega

