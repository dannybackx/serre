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

// Disabled this due to memory restriction on the Uno Wifi
#undef	USE_IFTTT

extern double		newPressure, newTemperature, oldPressure, oldTemperature;

extern int personal_timezone;
extern char *schedule_string;
extern int report_frequency_seconds;

/*
 * Time
 */
const int		buffer_size = 80;
extern char		buffer[];
extern struct tm	*tmnow;
extern time_t		boot_time, nowts;
extern const int	loop_delay;

extern void BMPInitialize();
extern int BMPQuery();

extern SFE_BMP180	*bmp;
extern Hatch		*hatch;
extern Light		*light;
extern Sunset		*sunset;

//
extern ELClientCmd	cmd;

// Light
extern const int light_treshold_high,
		 light_treshold_low,
		 light_min_duration,
		 hall_sensor_idle,
		 hall_sensor_trigger;

// ThingSpeak
extern ThingSpeak	*ts;

extern void ProcessCallback(void *client);
extern void WifiStatusCallback(void *response);

extern char reply[];
extern int lightSensorValue();

// Pin definitions
extern int	sensor_up_pin, sensor_down_pin, button_up_pin, button_down_pin, light_sensor_pin;
extern int	motor_dir1_pin, motor_dir2_pin, motor_speed_pin;
extern void ActivatePin(int, const char *);

// Sunset
extern char	*sunset_latitude, *sunset_longitude;

extern int	button_up, button_down;
extern int	sensor_up, sensor_down;

extern const char ts_url[];
extern const char ts_write_key[];
extern const char test_ts_write_key[];
extern const char noip_hostname[];
extern const char test_noip_hostname[];
extern const char noip_auth[];

extern "C" {
  char *strptime(const char *s, const char *format, struct tm *timeptr);
  extern void Debug(const char *format, ...);
}

extern void mqttSend(const char *);

extern ELClientMqtt mqtt;
extern void mqConnected(void *response);
extern void mqDisconnected(void *response);
extern void mqData(void *response);

extern void StartTrackStatus();
extern int ReadPin(int pin);

struct EspTestProduction {
  char *name;
  int test;
};
extern struct EspTestProduction esplist [];
