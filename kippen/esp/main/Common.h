/*
 * Common definitions
 *
 * Copyright (c) 2017, 2018, 2019 Danny Backx
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

#ifndef _ESPALARM_COMMON_H
#define _ESPALARM_COMMON_H

enum AlarmStatus {
  ARMED_OFF,		// Only ZONE_ALWAYS triggers the alarm
  ARMED_ON,		// Any sensor triggers the alarm
  ARMED_NIGHT,		// ZONE_SECURE sensors won't trigger alarm
  // ??
};

// A characteristic of both sensors and controllers
enum AlarmZone {
  ZONE_NONE,
  ZONE_SECURE,		// Sensors that you'll walk by at night
  			// A controller not in the perimeter (no need to authenticate)
  ZONE_PERIMETER,	// Most sensors are here
  			// Controllers require authentication before accepting actions
  ZONE_ALWAYS,		// Sensors that always trigger alarm (e.g. fire)
  ZONE_FROMPEER,	// Already evaluated, this is a real alarm passed from a peer controller
  ZONE_HID,		// Human interface : an rfid card, someone entered his pin, ...
  ZONE_REMOTE,		// Currently an internet connection, hopefully secure
};

enum buttonAction {
  BUTTON_NONE,
  BUTTON_PRESS,
  BUTTON_DEPRESS
};
#endif
