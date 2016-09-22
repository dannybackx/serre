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
#include "mywifi.h"

const int personal_timezone = +2;

const char *mqtt_server = MQTT_INT_SERVER;
const int mqtt_port = MQTT_INT_PORT;

const char *startup_text = "\n\nMQTT Automatic Chicken House Door\n"
	"(c) 2016 by Danny Backx\n\n";

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
 * Test results from testwemosd1/testwemosd1 :
 * Note this is with an old model Wemos D1 (http://www.wemos.cc/Products/d1.html).
 *	value		pin
 *	16		D2
 *	15		D10 / SS
 *	14		D13 / SCK / D5			Motor3
 *	13		D11 / MOSI / D7			D7
 *	12		D6 / MISO / D12			D12
 *	11		N/A
 *	10
 *	9
 *	8		crashes ?
 *	7
 *	6
 *	5		D15 / SCL / D3			SCL
 *	4		D14 / SDA / D4			D4
 *	3		D0 / RX				N/A (serial)
 *	2		TX1 / D9			Sensor1
 *	1		TX breaks serial connection	N/A (serial)
 *	0		D8				D8
 */

/*
 * Old style motor shield pin use :
 * D2, D13 not used
 * Always used = D4, D7, D8, D12
 * Servo : D9 (servo 1), D10 (servo 2)
 * DC Motors : D11 (motor 1), D3 (motor 2), D5 (motor 3), D6 (motor 4)
 *
 * My choice :
 *	use motor 4, so pin D6	(and D4 D7 D8 D12)
 *	-> available : D2, D13, D11, D3, D5
 */

/*
 * Pin definitions
 *
 * If the definition is < 0 then assume it's not hooked up.
 */
// Sensors that indicate when the hatch is at the up or down position
const int	sensor_up_pin = 2;			// D9
const int	sensor_down_pin = 15;			// D10

// i2c pins : to connect e.g. a BMP180
const int	sda_pin = 16;				// D2 (default value : 4)
const int	scl_pin = 5;				// Default value : 5

// A push button for manual override
const int	button_pin = 0;				// Analog-0

const int	pin_motor3 = 14;			// D13 / D5 / SCK
const int	pin_serial_tx = 1;			// D1, TX
const int	pin_serial_rx = 3;			// D0, RX
const int	pin_shield_1 = 4;			// D4
const int	pin_shield_2 = 13;			// D7
const int	pin_shield_3 = 0;			// D8
const int	pin_shield_4 = 12;			// D12

// Avail : GPIO16 / D2
