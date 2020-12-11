/*
 * This module manages unexpected disconnects (and recovery) from the network.
 *
 * Copyright (c) 2019, 2020 Danny Backx
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

#include <Arduino.h>

#include "secrets.h"
#include "StableTime.h"
#include "Network.h"
#include "App.h"

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <freertos/task.h>
#include <sys/socket.h>

Network::Network() {
  // FIXME get from config
  reconnect_interval = 30;

  qp_last_query = 0;
  qp_npeers = 0;
  status = NS_NONE;

  last_keepalive = -1;
  count_alives = 0;
  nopeers_counter = 0;

  last_mqtt_message_received = 0;

  restart_time = 0;
}

// Not really needed
Network::~Network() {
}

struct mywifi {
  const char *ssid, *pass, *bssid;
} mywifi[] = {
#ifdef MY_SSID_1
# ifdef MY_WIFI_BSSID_1
  { MY_SSID_1, MY_WIFI_PASSWORD_1, MY_WIFI_BSSID_1 },
# else
  { MY_SSID_1, MY_WIFI_PASSWORD_1, NULL },
# endif
#endif
#ifdef MY_SSID_2
# ifdef MY_WIFI_BSSID_2
  { MY_SSID_2, MY_WIFI_PASSWORD_2, MY_WIFI_BSSID_2 },
# else
  { MY_SSID_2, MY_WIFI_PASSWORD_2, NULL },
# endif
#endif
#ifdef MY_SSID_3
# ifdef MY_WIFI_BSSID_3
  { MY_SSID_3, MY_WIFI_PASSWORD_3, MY_WIFI_BSSID_3 },
# else
  { MY_SSID_3, MY_WIFI_PASSWORD_3, NULL },
# endif
#endif
#ifdef MY_SSID_4
# ifdef MY_WIFI_BSSID_4
  { MY_SSID_4, MY_WIFI_PASSWORD_4, MY_WIFI_BSSID_4 },
# else
  { MY_SSID_4, MY_WIFI_PASSWORD_4, NULL },
# endif
#endif
  { NULL, NULL, NULL}
};

const char *snetwork_tag = "Network static";

extern void ftp_init();

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
  switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;

    case SYSTEM_EVENT_GOT_IP6:
      ESP_LOGI(snetwork_tag, "We have an IPv6 address");
      // FIXME
      break;

    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGI(snetwork_tag, "SYSTEM_EVENT_STA_GOT_IP");

      network->setWifiOk(true);

      if (network) network->NetworkConnected(ctx, event);
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP_LOGE(snetwork_tag, "STA_DISCONNECTED, restarting");

      if (network) network->NetworkDisconnected(ctx, event);

      network->StopWifi();			// This also schedules a restart
      break;

    default:
      break;
  }
  return ESP_OK;
}

/*
 * This needs to be done before we can query the adapter MAC,
 * which we need to pass to Config.
 * After this, we can also await attachment to a network.
 */
void Network::SetupWifi(void)
{
  esp_err_t err;

  nopeers_counter = 0;
  mqtt_message = 0;

  tcpip_adapter_init();
  err = esp_event_loop_init(wifi_event_handler, NULL);
  if (err != ESP_OK) {
      /*
       * ESP_FAIL here means we've already done this, see components/esp32/event_loop.c :
       * esp_err_t esp_event_loop_init(system_event_cb_t cb, void *ctx)
       * {
       *     if (s_event_init_flag) {
       *         return ESP_FAIL;
       *     }
       */
      // ESP_LOGE(network_tag, "Failed esp_event_loop_init, reason %d", (int)err);

      // esp_restart();
      // return;
  }
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
      ESP_LOGE(network_tag, "Failed esp_wifi_init, reason %d", (int)err);
      // FIXME
      return;
  }
  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ESP_OK) {
      ESP_LOGE(network_tag, "Failed esp_wifi_set_storage, reason %d", (int)err);
      // FIXME
      return;
  }

  status = NS_SETUP_DONE;
}

void Network::WaitForWifi(void)
{
  ESP_LOGD(network_tag, "Waiting for wifi");
 
  wifi_config_t wifi_config;
  for (int ix = 0; mywifi[ix].ssid != 0; ix++) {
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, mywifi[ix].ssid);
    strcpy((char *)wifi_config.sta.password, mywifi[ix].pass);
    if (mywifi[ix].bssid) {
      int r = sscanf(mywifi[ix].bssid, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", 
        &wifi_config.sta.bssid[0],
        &wifi_config.sta.bssid[1],
        &wifi_config.sta.bssid[2],
        &wifi_config.sta.bssid[3],
        &wifi_config.sta.bssid[4],
        &wifi_config.sta.bssid[5]);
      wifi_config.sta.bssid_set = true;
      if (r != 6) {
	ESP_LOGE(network_tag, "Could not convert MAC %s into acceptable format", mywifi[ix].bssid);
	memset(wifi_config.sta.bssid, 0, sizeof(wifi_config.sta.bssid));
	wifi_config.sta.bssid_set = false;
      }
    } else
      memset(wifi_config.sta.bssid, 0, sizeof(wifi_config.sta.bssid));

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
      ESP_LOGE(network_tag, "Failed to set wifi mode to STA");
      // FIXME
      return;
    }
    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (err != ESP_OK) {
      ESP_LOGE(network_tag, "Failed to set wifi config");
      // FIXME
      return;
    }
    ESP_LOGI(network_tag, "Try wifi ssid [%s]", wifi_config.sta.ssid);
    err = esp_wifi_start();
    if (err != ESP_OK) {
      ESP_LOGE(network_tag, "Failed to start wifi");
      // FIXME
      return;
    }

    if (status == NS_SETUP_DONE) {
      for (int cnt = 0; cnt < 20; cnt++) {
        delay(100);
	ESP_LOGI(network_tag, ".. connected to wifi (attempt %d)", cnt+1);
	status = NS_CONNECTING;
        return;
      }
    } else {
	ESP_LOGE(network_tag, "Invalid status %d, expected %d", status, NS_SETUP_DONE);
    }
  }
}

// Flag the connection as ok
void Network::setWifiOk(boolean ok) {
  wifi_ok = ok;
  status = NS_RUNNING;
}

void Network::StopWifi() {
  esp_err_t err;

  ESP_LOGI(network_tag, "StopWifi");

  err = esp_wifi_disconnect();
  if (err != ESP_OK)
    ESP_LOGE(network_tag, "%s: esp_wifi_disconnect failed, reason %d (%s)", __FUNCTION__,
      err, esp_err_to_name(err));
  err = esp_wifi_stop();
  if (err != ESP_OK)
    ESP_LOGE(network_tag, "%s: esp_wifi_stop failed, reason %d (%s)", __FUNCTION__,
      err, esp_err_to_name(err));
  err = esp_wifi_deinit();
  if (err != ESP_OK)
    ESP_LOGE(network_tag, "%s: esp_wifi_deinit failed, reason %d (%s)", __FUNCTION__,
      err, esp_err_to_name(err));

  ScheduleRestartWifi();
}

/*
 * This function is called when we see a problem.
 */
void Network::disconnected(const char *fn, int line) {
  switch (status) {
  case NS_RUNNING:
    // char msg[200];
    // sprintf(msg, "Network is not connected (caller: %s line %d)", fn, line);
    // ESP_LOGE(network_tag, (char *)msg);
    ESP_LOGE(network_tag, "Network is not connected (caller: %s line %d)", fn, line);

    if (strcmp(fn, "mqtt_event_handler") == 0) {
#if 1
      ScheduleRestartWifi();
#else
      status = NS_FAILED;
      last_connect = stableTime->Query();
      wifi_ok = false;

      SetupWifi();
#endif
      return;
    }
    status = NS_FAILED;
    last_connect = stableTime->Query();
    wifi_ok = false;
    return;
  case NS_NONE:
  case NS_CONNECTING:
    return;
  case NS_SETUP_DONE:
  case NS_FAILED:
    break;
  }

#if 0
  // FAILED --> ..
  time_t now = stableTime->Query();
  time_t diff = now - last_connect;

  if (now - last_connect > reconnect_interval) {
    if ((diff % reconnect_interval) == 0) {

      ESP_LOGE(network_tag, "Reconnecting WiFi ..");

      SetupWifi();
      // FIXME
    }
  }
#else
  ScheduleRestartWifi();
#endif
}

void Network::eventDisconnected(const char *fn, int line) {
    ESP_LOGE(network_tag, "Disconnect event (caller: %s line %d)", fn, line);
}

void Network::NetworkConnected(void *ctx, system_event_t *event) {
}

void Network::NetworkDisconnected(void *ctx, system_event_t *event) {
}


/*
 * Check whether the broadcast at startup to find peers was succesfull.
 */
void Network::loop(time_t now) {
  NoPeerLoop(now);
  LoopRestartWifi(now);
}

bool Network::isConnected() {
  return (status == NS_RUNNING);
}

void Network::GotDisconnected(const char *fn, int line) {
  ESP_LOGE(network_tag, "Network is not connected (caller: %s line %d)", fn, line);
  status = NS_NONE;
}

void Network::StartKeepAlive(time_t now) {
  if (last_keepalive != -1 && count_alives == 0) {
#if 1
    ESP_LOGD(network_tag, "StartKeepAlive: no feedback since last query, restarting network -> ignored");
#else
    ESP_LOGE(network_tag, "StartKeepAlive: no feedback since last query, restarting network");
    SetupWifi();
    return;
#endif
  }
  count_alives = 0;
  last_keepalive = now;
}

void Network::ReceiveKeepAlive() {
  count_alives++;
}

void Network::Report() {
  ESP_LOGD(network_tag, "Report: status %s, npeers %d",
    (status == NS_RUNNING) ? "NS_RUNNING" :
    (status == NS_FAILED) ? "NS_FAILED" :
    (status == NS_NONE) ? "NS_NONE" :
    (status == NS_CONNECTING) ? "NS_CONNECTING" : "?",
    qp_npeers);
}

/*
 * Input from MQTT
 */
void Network::mqttSubscribed() {
  ESP_LOGD(network_tag, "MQTT subscribed");
}

void Network::mqttUnsubscribed() {
  ESP_LOGE(network_tag, "MQTT unsubscribed");
}

void Network::mqttDisconnected() {
  if (mqtt_message < 3)
    ESP_LOGE(network_tag, "MQTT disconnected");
  mqtt_message++;

  // FIX ME do something
  ScheduleRestartWifi();
}

void Network::mqttConnected() {
  ESP_LOGD(network_tag, "MQTT connected");
}

void Network::gotMqttMessage() {
  ESP_LOGD(network_tag, "Got MQTT message");

  last_mqtt_message_received = stableTime->Query();
}

/*
 * Input from QueryPeers
 */
// Upon QueryPeers(), reset counter, remember timestamp
void Network::DoQueryPeers() {
  qp_last_query = stableTime->Query();
  // qp_npeers = 0;
  qp_message = 0;
}

/*
 * We're being told we are adding a peer, parameter indicates it was already known to us.
 *
 * FIX ME a future version might want to pick up a timestamp of this call, not just ignore
 * adding known peers.
 */
void Network::GotPeer(bool known) {
  ESP_LOGD(network_tag, "%s(%s)", __FUNCTION__, known ? "known" : "new");

  if (! known)
    qp_npeers++;
}

// Repeat query if nobody replied to it
void Network::NoPeerLoop(time_t now) {
}

/*
 * (delayed) Restart handler
 */
void Network::ScheduleRestartWifi() {
  if (restart_time != 0) {
    // ESP_LOGE(network_tag, "%s: restart_time already set : %ld", __FUNCTION__, restart_time);
    return;
  }

  time_t now = stableTime->Query();
  restart_time = now + 2;

  ESP_LOGI(network_tag, "%s : set restart_time to %ld", __FUNCTION__, restart_time);
}

void Network::LoopRestartWifi(time_t now) {
  if (restart_time == 0)
    return;
  if (restart_time <= now) {
    RestartWifi();
    restart_time = 0;			// FIX ME maybe we should have a 2nd timeout to verify
  }
}

void Network::RestartWifi() {
  ESP_LOGI(network_tag, "RestartWifi");

  SetupWifi();
  WaitForWifi();
}
