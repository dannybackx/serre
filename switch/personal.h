/*
 * Power switch, controlled by MQTT
 *
 * Copyright (c) 2016, 2017, 2021 Danny Backx
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

/* Personal configuration that are not secrets, so moved from secrets.h */
#ifdef DEBUG
# define MQTT_CLIENT	"esp switch debug"
# define SWITCH_TOPIC	"/testswitch"
# define OTA_ID		"Switch-debug"
#else
# define MQTT_CLIENT	"esp switch"
# define SWITCH_TOPIC	"/switch"
# define OTA_ID		"Switch"
#endif

#define	SSR_PIN		D3
#define	LED_PIN		D4

#define	MY_TIMEZONE	TZ_Europe_Brussels
#define	MY_NTP_SERVER	"ntp.telenet.be"
