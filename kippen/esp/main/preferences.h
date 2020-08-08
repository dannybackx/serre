/*
 * Public preferences file for Alarm controller
 *
 * Copyright (c) 2017, 2018, 2019 Danny Backx
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

// Configuration file name to read from SPIFFS
#define	PREF_CONFIG_FN	"config.json"

// OpenWeatherMap
#define	PREF_OWM_API_SRV	"api.openweathermap.org"
#define	PREF_OWM_QUERY_PATTERN	"http://%s/data/2.5/weather?q=%s,%s&APPID=%s&units=metric"
#define	PREF_OWM_ICON_PATTERN	"http://openweathermap.org/img/w/%s.png"

// Number of buffers for the Clock module
#define	PREF_CLOCK_NB	4

// Number of buffers for the Weather module
#define	PREF_WEATHER_NB	4

// Wunderground query frequency in minutes
// #define	WU_TIME_OK	90
#define	WU_TIME_OK	10
// #define	WU_TIME_FAIL	15
#define	WU_TIME_FAIL	2

// Backlight timeout, in seconds
#define	PREF_BACKLIGHT_TIMEOUT	10

// Timeout for removing nodes : 15 minutes
#define	PREF_NODE_FORGET_TIMEOUT	(60 * 15)
