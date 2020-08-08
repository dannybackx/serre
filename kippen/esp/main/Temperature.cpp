/*
 * This module manages local temperature (etc) sensors.
 * This is not a per sensor class, it manages a list of sensors.
 * The sensors are meant to be connected via SPI or I²C.
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
 *
 * Supported sensors :
 * - mcp9808
 */

#include "Kippen.h"
#include "Temperature.h"

Temperature::Temperature() {
  mcp = 0;
  count = 0;
  reportts1 = 0;

  // Note relies on having I²C already active from the caller

  MCP9808();

  if (mcp)
    mcp->wake();
}

Temperature::~Temperature() {
  if (mcp)
    delete mcp;
  mcp = 0;
}

/*
 * Report environmental information periodically
 *
 * There are two slow down mechanisms here.
 * - As temperature doesn't change quickly (use other sensors to guard against house fire),
 *   only act with large intervals. The interval is hardcoded to a minute.
 * - Reporting to peers follows this schedule.
 * - Reporting (logging) over MQTT is still slower : every 5 minutes.
 */
void Temperature::loop(time_t nowts) {
  char msg[128], ts[24];

  // Don't do this too often : temperature doesn't change that quickly
  if (nowts - oldts < 59 && count > 0)
    return;
  oldts = nowts;
  count++;

  struct tm *tmp = localtime(&nowts);
  strftime(ts, sizeof(ts), "%Y-%m-%d %T", tmp);

  if (mcp) {
    temp_c = mcp->readTempC();
    if (count == 1)
      oldvalue = temp_c;

    ESP_LOGE(temperature_tag, "MCP : %2.4f", temp_c);

    // Only record differences above some level
    float diff = (temp_c < oldvalue) ? oldvalue - temp_c : temp_c - oldvalue;
    if (diff > 0.26)
      oldvalue = temp_c;

    sprintf(msg, "Temperature %2.2f °C (%s, mcp)", temp_c, ts);

    // Report at sudden temperature differences, or every five minutes
    if (diff > 1.0 || nowts - reportts1 > 300) {
      kippen->Report(msg);
      reportts1 = nowts;
    }
  }
}

void Temperature::MCP9808() {
  if (mcp == 0)
    mcp = new Adafruit_MCP9808();

  for (int i=0x18; i<0x20; i++)
    if (mcp->begin(i)) {
      ESP_LOGI(temperature_tag, "MCP9808 detected at 0x%02x", i);
      addr = i;

      mcp->setResolution(3);	// resolution 0. 625°C , 250 ms sample time
      mcp->wake();

      return;
    }

  ESP_LOGE(temperature_tag, "No MCP9808 detected");

  delete mcp;
  mcp = 0;
}
