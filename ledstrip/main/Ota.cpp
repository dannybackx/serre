/*
 * ESP32 OTA code
 *
 * Copyright (c) 2018, 2019, 2020 Danny Backx
 *   Derived from the OTA example code in esp-idf/examples/system/ota/simple_ota_example
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

#include "sdkconfig.h"
#include "Ota.h"
#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_https_ota.h>

static esp_err_t http_event_handler(esp_http_client_event_t *evt);

Ota::Ota() {
}

extern const char cert_pem_start[] asm("_binary_dannybackx_hopto_org_crt_start");

/*
 * Allow us to charge up something like ota://host:port/path/to/file
 */
void Ota::DoOTA(const char *url) {
  esp_http_client_config_t otacfg;

  memset(&otacfg, 0, sizeof(otacfg));
  otacfg.url = url;
  otacfg.event_handler = http_event_handler;
  otacfg.cert_pem = cert_pem_start;

  ESP_LOGI(ota_tag, "%s trying to restart %s", __FUNCTION__, url);

  esp_err_t err = esp_https_ota(&otacfg);
  if (err == ESP_OK) {
    ESP_LOGI(ota_tag, "%s trying to restart %s", __FUNCTION__, url);
    esp_restart();
  } else {
    ESP_LOGE(ota_tag, "OTA failed %d %s", err, esp_err_to_name(err));
  }
}

void Ota::DoOTA()
{
  DoOTA(CONFIG_OTA_URL);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  const char *ota_tag = "OTA";

  switch(evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(ota_tag, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(ota_tag, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(ota_tag, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(ota_tag, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(ota_tag, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(ota_tag, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(ota_tag, "HTTP_EVENT_DISCONNECTED");
    break;
  }
  return ESP_OK;
}

