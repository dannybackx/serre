/*
 * ESP32 OTA code
 *
 * Copyright (c) 2018, 2019, 2020 Danny Backx
 *   Derived from the OTA example code in esp-idf/examples/system/ota/main/ota_example_main.c .
 *   The original code is in the Public Domain, changes are subject to the GNU LGPL (see below).
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
#ifndef	__KEYPAD_OTA_H_
#define	__KEYPAD_OTA_H_

class Ota {
  public:
    Ota();
    void DoOTA();
    void DoOTA(const char *url);
  private:
    const char *ota_tag = "OTA";
};
#endif  // __KEYPAD_OTA_H_
