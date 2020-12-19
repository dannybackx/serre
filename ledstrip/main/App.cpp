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
#include "Mqtt.h"
#include "FastLED.h"

const char		*app_tag = "LED strip";

Network			*network = 0;
Ota			*ota = 0;
time_t			nowts, boot_time = 0;
Mqtt			*mqtt = 0;
char			*boot_msg = 0;
TaskHandle_t		tsk = 0;

/*
 * FastLED stuff
 */
CRGBArray<MY_NUM_LEDS> leds;
extern CRGBPalette16 gTargetPalette;
extern void chooseNextColorPalette(CRGBPalette16& pal);

void GeneralLEDsetup();
extern void twinklefox_loop();
extern void star_loop(int);
extern void cylon_loop();
void led_loop(void *);

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

  GeneralLEDsetup();

  xTaskCreate(&led_loop, "led task", 8192, NULL, 5, &tsk);
}

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

  vTaskDelay(10);

  struct tm *tmp = localtime(&nowts);
  int hour = tmp->tm_hour * 100 + tmp->tm_min;
  if (hour > 2330 && tsk != 0) {
    vTaskDelete(tsk);
    tsk = 0;
  }
  if (hour > 700 && tsk == 0) {
    xTaskCreate(&led_loop, "led task", 8192, NULL, 5, &tsk);
  }
}

void GeneralLEDsetup() {
  ESP_LOGE(app_tag, "LED strip setup");

  // delay( 3000 ); //safety startup delay

  FastLED.setMaxPowerInVoltsAndMilliamps(5 /* V */, 1000 /* mA */);
  FastLED.addLeds<MY_LED_TYPE, MY_DATA_PIN, MY_COLOR_ORDER>(leds, MY_NUM_LEDS).setCorrection(TypicalLEDStrip);

  chooseNextColorPalette(gTargetPalette);
}

void led_loop(void *ptr) {
  while (1) {
    struct tm *tmp = localtime(&nowts);

    /*
     * A simple way to have a sequence of patterns : change every minute
     * The modulo operand should be as high as the number of patterns
     *
     * We're cheating because I want to twinkle more than the other patterns, hence 6.
     */
    int ix = tmp->tm_min % 6;

    switch (ix) {
    case 0:
    case 2:
    case 4:
      twinklefox_loop();
      break;
    case 1:
      cylon_loop();
      break;
    case 3:
      star_loop(1);
      break;
    case 5:
      star_loop(2);
      break;
    }
  }
  // Should never fall through
  vTaskDelete(0);	// This prevents a FreeRTOS panic (upon return from a task loop)
}
