/*
 * Main application for esp32
 *
 * Copyright (c) 2017, 2018, 2019, 2020 Danny Backx
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

#include "App.h"
#include "Network.h"
#include "StableTime.h"
#include "App.h"
#include "Mqtt.h"

const char *app_tag = "LED strip";

Network			*network = 0;
Ota			*ota = 0;
time_t			nowts, boot_time = 0;
Mqtt			*mqtt = 0;

void TimeSync(struct timeval *);

// Initial function
void setup(void) {
  ESP_LOGI(app_tag, "LED strip (c) 2020 Danny Backx");
#if 1
  extern const char *build;
  ESP_LOGI(app_tag, "Build timestamp %s", build);
#endif

  /* Network */
  network = new Network();
  network->RegisterModule(app_tag, 0, 0, TimeSync);

  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  ESP_LOGI(app_tag, "ESP32 chip with %d CPU cores, WiFi%s%s, silicon revision %d",
    chip_info.cores,
    (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
    (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
    chip_info.revision);

  ESP_LOGI(app_tag, "IDF version %s (build v%d.%d)", esp_get_idf_version(), IDF_MAJOR_VERSION, IDF_MINOR_VERSION);

  // Make stuff from the underlying libraries quieter
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_ERROR);
  esp_log_level_set("wifi", ESP_LOG_ERROR);
  esp_log_level_set("system_api", ESP_LOG_ERROR);

				ESP_LOGD(app_tag, "Starting WiFi "); 
  // First stage, so we can query the MAC
  network->SetupWifi();

  ota = new Ota();
  mqtt = new Mqtt();

  /*
   * Set up the time
   *
   * See https://www.di-mgt.com.au/wclock/help/wclo_tzexplain.html for examples of TZ strings.
   */
  // stableTime = new StableTime("EST5EDT,M3.2.0/2,M11.1.0");	// US
  stableTime = new StableTime("CET-1CEST,M3.5.0/2,M10.5.0/3");	// Europe

  network->WaitForWifi();

  extern void twinklefox_setup();
  twinklefox_setup();
}

char *boot_msg = 0;

// Record boot time
void TimeSync(struct timeval *tvp) {
  ESP_LOGD(app_tag, "TimeSync");

  if (boot_time == 0) {
    nowts = boot_time = tvp->tv_sec;

    char *ts = stableTime->TimeStamp(boot_time);
    char *boot_msg = (char *)malloc(80);
    sprintf(boot_msg, "boot at %s", ts);
  }
}

void loop()
{
  nowts = stableTime->loop();

  if (network) network->loop(nowts);

  if (boot_time != 0 && boot_msg != 0) {
    ESP_LOGI(app_tag, "%s", boot_msg);
    mqtt->Report(boot_msg);
    free((void *)boot_msg);
    boot_msg = 0;
  }

  extern void twinklefox_loop();
  twinklefox_loop();
}
