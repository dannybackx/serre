/*
 * This module manages temperature (etc) sensors
 *
 * Copyright (c) 2020 Danny Backx
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

#ifndef	_TEMPERATURE_SENSOR_H_
#define	_TEMPERATURE_SENSOR_H_

// Temperature
#include "Adafruit_MCP9808.h"

class Temperature {
public:
  Temperature();
  ~Temperature();
  void loop(time_t);

  // Initialize a mcp9808
  void MCP9808();
  void MCP9808(int);

private:
  Adafruit_MCP9808	*mcp;
  int			addr;
  int			count;
  float			oldvalue, temp_c;
  time_t		oldts, reportts1;

  const char		*temperature_tag = "Temperature";
};

#endif	/* _TEMPERATURE_SENSOR_H_ */
