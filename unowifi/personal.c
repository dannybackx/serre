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
int personal_timezone = +2;

int verbose = 0;

/*
 * Schedule for hatch
 *	format is CSV
 *	hour,updown
 */
char *schedule_string = "21:00,-1,07:35,1";

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

/*
 * Old style motor shield pin use :
 * D2, D13 not used
 * Always used = D4, D7, D8, D12
 * Servo : D9 (servo 1), D10 (servo 2)
 * DC Motors : D11 (motor 1), D3 (motor 2), D5 (motor 3), D6 (motor 4)
 *
 * My choice :
 *	use motor 3, so pin D5	(and D4 D7 D8 D12)
 *	-> available : D2, D13, D11, D3, D6
 */

/*
 * Pin definitions
 *
 * If the definition is < 0 then assume it's not hooked up.
 */
// Sensors that indicate when the hatch is at the up or down position
const int	sensor_up_pin =		2;
const int	sensor_down_pin =	-3;

const int	button_up_pin =		-11;
const int	button_down_pin =	-13;

// Still available : D6, A0 .. A3
