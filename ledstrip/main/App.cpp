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

// Initial function
void setup(void) {
  ESP_LOGI(app_tag, "LED strip (c) 2020 Danny Backx");
#if 1
  extern const char *build;
  ESP_LOGI(app_tag, "Build timestamp %s", build);
#endif

  /* Network */
  network = new Network();

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
   * This one works for Europe : CET-1CEST,M3.5.0/2,M10.5.0/3
   * I assume that this one would work for the US : EST5EDT,M3.2.0/2,M11.1.0
   */
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  stableTime = new StableTime();

  network->WaitForWifi();

  extern void twinklefox_setup();
  twinklefox_setup();
}

void loop()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  if (stableTime) stableTime->loop(&tv);

  nowts = tv.tv_sec;

  // Record boot time
  if (boot_time == 0 && nowts > 15) {
    boot_time = nowts;

    char msg[80], ts[24];
    struct tm *tmp = localtime(&boot_time);
    strftime(ts, sizeof(ts), "%Y-%m-%d %T", tmp);
    sprintf(msg, "Boot at %s", ts);

    if (mqtt) mqtt->Report(msg);
    ESP_LOGE(app_tag, "%s", msg);
  }

  if (network) network->loop(nowts);

  extern void twinklefox_loop();
  twinklefox_loop();
}
