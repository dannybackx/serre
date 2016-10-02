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

extern double		newPressure, newTemperature, oldPressure, oldTemperature;

extern int personal_timezone;
extern char *schedule_string;
extern int report_frequency_seconds;

/*
 * Time
 */
extern char		buffer[];
extern struct tm	*tmnow;

extern void BMPInitialize();
extern int BMPQuery();
extern SFE_BMP180	*bmp;
extern Hatch		*hatch;

extern void ProcessCallback(WifiData client);

extern char reply[];

// Pin definitions
extern int	sensor_up_pin, sensor_down_pin, button_up_pin, button_down_pin;
extern void ActivatePin(int, const char *);

extern const char timedate_fmt[];
extern const char sensor_up_string[];
extern const char sensor_down_string[];
extern const char button_up_string[];
extern const char button_down_string[];
extern const char bmp_fmt[];
extern const char no_sensor_string[];
extern const char setting_rtc[];
extern const char initializing_bmp180[];
extern const char starting_wifi[];
extern const char startup_text1[];
extern const char startup_text2[];
extern const char unmatched_command[];

extern const char time_query[];
extern const char rcmd_time_set[];
extern const char rcmd_timezone_set[];

extern const char query_bmp180[];
extern const char hatch_query[];
extern const char hatch_up[];
extern const char hatch_down[];
extern const char hatch_stop[];
extern const char hatch_use_motor[];
extern const char hatch_state_fmt[];

extern const char set_schedule_to[];
extern const char schedule_query[];
extern const char schedule_update[];
extern const char schedule[];

extern const char version_query[];
extern const char no_info[];
extern const char server_build[];
extern const char rtc_failure[];

extern char progmem_bfr[];
extern "C" {
  const char *gpm(const char *p);
}
