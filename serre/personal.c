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
const char *mqtt_server = "192.168.1.185";
const int mqtt_port = 1883;

const char *mqtt_topic_serre =		"Serre";
const char *mqtt_topic_bmp180 =		"Serre/BMP180";
const char *mqtt_topic_valve =		"Serre/Valve";
const char *mqtt_topic_valve_start =	"Serre/Valve/1";
const char *mqtt_topic_valve_stop =	"Serre/Valve/0";
const char *mqtt_topic_report =		"Serre/Report";
const char *mqtt_topic_report_freq =	"Serre/Report/Frequency";
const char *mqtt_topic_restart =	"Serre/System/Reboot";
const char *mqtt_topic_reconnects =	"Serre/System/Reconnects";
const char *mqtt_topic_boot_time =	"Serre/System/Boot";
const char *mqtt_topic_current_time =	"Serre/System/Time";
const char *mqtt_topic_schedule_set =	"Serre/Schedule/Set";
const char *mqtt_topic_schedule =	"Serre/Schedule";
const char *mqtt_topic_verbose =	"Serre/System/Verbose";
const char *mqtt_topic_version =	"Serre/System/Version";
const char *mqtt_topic_info =		"Serre/System/Info";

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
// char *watering_schedule_string = "21:00,1,21:15,0";
char *watering_schedule_string = "16:27,1,16:28,0";

/*
 * NTP servers
 */
char	*ntp_server_1 = "ntp.scarlet.be",
	*ntp_server_2 = "ntp.belnet.be";
