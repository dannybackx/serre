/*
 * Generic app include file
 *
 * Copyright (c) 2018, 2019, 2020 Danny Backx
 *
 * License (GNU Lesser General Public License) :
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 3 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _APP_H_
#define _APP_H_

#define TS_NIGHT	2330
#define	TS_MORNING	700
#define MY_DATA_PIN	16
#define	LEDSTRIP	"ledstrip"

#include "secrets.h"
#include <Ota.h>
#include <esp_log.h>
#include <FastLED.h>

#include <apps/sntp/sntp.h>

extern Ota			*ota;
extern bool			OTAbusy;

extern time_t			nowts, boot_time;

extern const char		*build;

#define MY_NUM_LEDS		60
#define MY_NUM_LEDS_ALLOC	(MY_NUM_LEDS + 40)
#define MY_LED_TYPE		WS2811
#define MY_COLOR_ORDER		GRB

extern CRGBArray<MY_NUM_LEDS_ALLOC> leds;

#endif /* _APP_H_ */
