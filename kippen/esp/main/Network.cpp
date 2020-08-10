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

#include "Kippen.h"

#include "secrets.h"
#include "Acme.h"
#include "Network.h"
#include "WebServer.h"

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include "mqtt_client.h"
#include <freertos/task.h>
#include <sys/socket.h>

#include <esp_event_legacy.h>
#include "esp_wpa2.h"

WebServer		*ws;

Network::Network() {
  reconnect_interval = 30;

  status = NS_NONE;

  last_mqtt_message_received = 0;

  restart_time = 0;
  ws = 0;
}

// Not really needed
Network::~Network() {
  if (ws)
    delete ws;
}

/*
 * You should define macros in secrets.h, like this :
 * #define WIFI_1_SSID             "Telenet-44778855"
 * #define WIFI_1_PASSWORD         "yeah,right"
 * #define WIFI_1_BSSID            NULL
 * #define WIFI_1_NAT              true
 * #define WIFI_1_EAP_IDENTITY     NULL
 * #define WIFI_1_EAP_PASSWORD     NULL
 *
 * You can do so for up to 6 access point (or extend the table below).
 * Your typical home situation would look like the entry above. Your typical enterprise network
 * would have NULL for the regular password, but would have EAP entries.
 * The BSSID field can be used to select a specific access point, if you have more than one with the same SSID.
 *
 * The discard and counter fields are for internal use, initialize them to false and 0 respectively.
 */
struct mywifi {
  const char *ssid, *pass, *bssid;
  const char *eap_identity, *eap_password;
  bool nat;
  bool discard;
  int counter;
} mywifi[] = {
#ifdef WIFI_1_SSIDx
  { WIFI_1_SSID, WIFI_1_PASSWORD, WIFI_1_BSSID, WIFI_1_EAP_IDENTITY, WIFI_1_EAP_PASSWORD, WIFI_1_NAT, false, 0 },
#endif
#ifdef WIFI_2_SSID
  { WIFI_2_SSID, WIFI_2_PASSWORD, WIFI_2_BSSID, WIFI_2_EAP_IDENTITY, WIFI_2_EAP_PASSWORD, WIFI_2_NAT, false, 0 },
#endif
#ifdef WIFI_3_SSID
  { WIFI_3_SSID, WIFI_3_PASSWORD, WIFI_3_BSSID, WIFI_3_EAP_IDENTITY, WIFI_3_EAP_PASSWORD, WIFI_3_NAT, false, 0 },
#endif
#ifdef WIFI_4_SSID
  { WIFI_4_SSID, WIFI_4_PASSWORD, WIFI_4_BSSID, WIFI_4_EAP_IDENTITY, WIFI_4_EAP_PASSWORD, WIFI_4_NAT, false, 0 },
#endif
#ifdef WIFI_5_SSID
  { WIFI_5_SSID, WIFI_5_PASSWORD, WIFI_5_BSSID, WIFI_5_EAP_IDENTITY, WIFI_5_EAP_PASSWORD, WIFI_5_NAT, false, 0 },
#endif
#ifdef WIFI_6_SSID
  { WIFI_6_SSID, WIFI_6_PASSWORD, WIFI_6_BSSID, WIFI_6_EAP_IDENTITY, WIFI_6_EAP_PASSWORD, WIFI_6_NAT, false, 0 },
#endif
  /*
   * This should be the last entry
   */
  { NULL,        NULL,            NULL,         NULL,                NULL,                false,      false, 0 }
};

const char *snetwork_tag = "Network static";

extern void ftp_init();

static const char *EventId2String(int eid) {
  switch (eid) {
  case SYSTEM_EVENT_STA_START:			return "START";
  case SYSTEM_EVENT_STA_STOP:			return "STOP";
  case SYSTEM_EVENT_GOT_IP6:			return "GOT_IP6";
  case SYSTEM_EVENT_STA_DISCONNECTED:		return "DISCONNECTED";
  case SYSTEM_EVENT_STA_CONNECTED:		return "CONNECTED";
  case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:	return "AUTHMODE_CHANGE";
  case SYSTEM_EVENT_STA_GOT_IP:			return "GOT_IP";
  case SYSTEM_EVENT_STA_LOST_IP:		return "LOST_IP";
  case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:		return "WPS_ER_SUCCESS";
  case SYSTEM_EVENT_STA_WPS_ER_FAILED:		return "WPS_ER_FAILED";
  case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:		return "WPS_ER_TIMEOUT";
  case SYSTEM_EVENT_STA_WPS_ER_PIN:		return "WPS_ER_PIN";
  case SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP:	return "WPS_ER_PBC_OVERLAP";
  default:					return "?";
  }
}

static const char *WifiReason2String(int r) {
  switch (r) {
  case WIFI_REASON_UNSPECIFIED:			return "UNSPECIFIED";
  case WIFI_REASON_AUTH_EXPIRE:			return "AUTH_EXPIRE";
  case WIFI_REASON_AUTH_LEAVE:			return "AUTH_LEAVE";
  case WIFI_REASON_ASSOC_EXPIRE:		return "ASSOC_EXPIRE";
  case WIFI_REASON_ASSOC_TOOMANY:		return "ASSOC_TOOMANY";
  case WIFI_REASON_NOT_AUTHED:			return "NOT_AUTHED";
  case WIFI_REASON_NOT_ASSOCED:			return "NOT_ASSOCED";
  case WIFI_REASON_ASSOC_LEAVE:			return "ASSOC_LEAVE";
  case WIFI_REASON_ASSOC_NOT_AUTHED:		return "ASSOC_NOT_AUTHED";
  case WIFI_REASON_DISASSOC_PWRCAP_BAD:		return "DISASSOC_PWRCAP_BAD";
  case WIFI_REASON_DISASSOC_SUPCHAN_BAD:	return "DISASSOC_SUPCHAN_BAD";
  case WIFI_REASON_IE_INVALID:			return "IE_INVALID";
  case WIFI_REASON_MIC_FAILURE:			return "MIC_FAILURE";
  case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:	return "4WAY_HANDSHAKE_TIMEOUT";
  case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:	return "GROUP_KEY_UPDATE_TIMEOUT";
  case WIFI_REASON_IE_IN_4WAY_DIFFERS:		return "IE_IN_4WAY_DIFFERS";
  case WIFI_REASON_GROUP_CIPHER_INVALID:	return "GROUP_CIPHER_INVALID";
  case WIFI_REASON_PAIRWISE_CIPHER_INVALID:	return "PAIRWISE_CIPHER_INVALID";
  case WIFI_REASON_AKMP_INVALID:		return "AKMP_INVALID";
  case WIFI_REASON_UNSUPP_RSN_IE_VERSION:	return "UNSUPP_RSN_IE_VERSION";
  case WIFI_REASON_INVALID_RSN_IE_CAP:		return "INVALID_RSN_IE_CAP";
  case WIFI_REASON_802_1X_AUTH_FAILED:		return "802_1X_AUTH_FAILED";
  case WIFI_REASON_CIPHER_SUITE_REJECTED:	return "CIPHER_SUITE_REJECTED";
  case WIFI_REASON_BEACON_TIMEOUT:		return "BEACON_TIMEOUT";
  case WIFI_REASON_NO_AP_FOUND:			return "NO_AP_FOUND";
  case WIFI_REASON_AUTH_FAIL:			return "AUTH_FAIL";
  case WIFI_REASON_ASSOC_FAIL:			return "ASSOC_FAIL";
  case WIFI_REASON_HANDSHAKE_TIMEOUT:		return "HANDSHAKE_TIMEOUT";
  case WIFI_REASON_CONNECTION_FAIL:		return "CONNECTION_FAIL";
  default:					return "?";
  }
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
  ESP_LOGE(snetwork_tag, "wifi_event_handler(%d,%s)", event->event_id, EventId2String(event->event_id));

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
      if (kippen) kippen->NetworkConnected(ctx, event);
      if (acme && ! network->NetworkIsNatted()) {
        // Note only start running ACME if we're *not* in a NATted environment
        acme->NetworkConnected(ctx, event);

        if (! acme->HaveValidCertificate()) {
          ESP_LOGI(snetwork_tag, "Don't have a valid certificate ...");
          acme->CreateNewAccount();
          acme->CreateNewOrder();
        }
      }
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:

      ESP_LOGI(snetwork_tag, "SYSTEM_EVENT_STA_DISCONNECTED");
      if (network->getStatus() == NS_CONNECTING) {
        system_event_sta_disconnected_t *evp = &event->event_info.disconnected;
        /*
	 * This is the asynchronous reply of a failed connection attempt.
	 * If this means a network should be discarded, do so.
	 * After that, start scanning again.
	 */

        ESP_LOGE(snetwork_tag, "Failed to connect to this SSID (reason %d %s)",
	  evp->reason, WifiReason2String(evp->reason));

	network->setStatus(NS_FAILED);
	esp_wifi_stop();

	switch (evp->reason) {
	case WIFI_REASON_NO_AP_FOUND:	// FIX ME probably more than just this case
	case WIFI_REASON_AUTH_FAIL:
          network->setReason(evp->reason);
	  network->DiscardCurrentNetwork();
	  break;
	default:
	  break;
	}

	// Trigger next try
	network->setStatus(NS_SETUP_DONE);
	network->SetupWifi();
        network->WaitForWifi();
      } else {
	/*
	 * We were connected but lost the network. So gracefully shut down open connections,
	 * and then try to reconnect to the network.
	 */
        ESP_LOGI(snetwork_tag, "STA_DISCONNECTED, restarting");

	if (kippen) kippen->NetworkDisconnected(ctx, event);
	if (acme) acme->NetworkDisconnected(ctx, event);
	if (network) network->NetworkDisconnected(ctx, event);

        network->StopWifi();			// This also schedules a restart
      }
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
  wifi_config_t wifi_config;
  esp_err_t err;

  ESP_LOGI(network_tag, "Waiting for wifi");
 
  for (int ix = 0; mywifi[ix].ssid != 0; ix++) {
    ESP_LOGI(network_tag, "Wifi %d, ssid [%s]", ix, mywifi[ix].ssid);
    if (mywifi[ix].discard) {
      ESP_LOGD(network_tag, "Discarded SSID \"%s\"", mywifi[ix].ssid);
      continue;
    }

    // Discard an entry if we've unsuccesfully tried it several times ...
    if (mywifi[ix].counter++ >= 3) {
      mywifi[ix].discard = true;
      ESP_LOGE(network_tag, "Discarded SSID \"%s\", counter %d", mywifi[ix].ssid, mywifi[ix].counter);
      continue;
    }

    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, mywifi[ix].ssid);

    if (mywifi[ix].bssid) {
      int r = sscanf(mywifi[ix].bssid, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", 
	&wifi_config.sta.bssid[0], &wifi_config.sta.bssid[1], &wifi_config.sta.bssid[2],
	&wifi_config.sta.bssid[3], &wifi_config.sta.bssid[4], &wifi_config.sta.bssid[5]);
      wifi_config.sta.bssid_set = true;
      if (r != 6) {
	ESP_LOGE(network_tag, "Could not convert MAC %s into acceptable format", mywifi[ix].bssid);
	memset(wifi_config.sta.bssid, 0, sizeof(wifi_config.sta.bssid));
	wifi_config.sta.bssid_set = false;
      }
    } else
      memset(wifi_config.sta.bssid, 0, sizeof(wifi_config.sta.bssid));

    /*
     * Normal version : use WPA
     */
    if (mywifi[ix].pass) {
      ESP_LOGI(network_tag, "Wifi %d, ssid [%s], has WPA config, pwd [%s]",
        ix, mywifi[ix].ssid, mywifi[ix].pass);

      strcpy((char *)wifi_config.sta.password, mywifi[ix].pass);
    } else if (mywifi[ix].eap_password && strlen(mywifi[ix].eap_password) > 0) {
      /*
       * WPA2
       */
      ESP_LOGI(network_tag, "Wifi %d, ssid [%s], WPA2", ix, mywifi[ix].ssid);

      err = esp_wifi_sta_wpa2_ent_set_identity((const unsigned char *)mywifi[ix].eap_identity,
        strlen(mywifi[ix].eap_identity));
      if (err != ESP_OK) {
        ESP_LOGE(network_tag, "Error %d setting WPA2 identity, %s", err, esp_err_to_name(err));
	continue;
      } else
        ESP_LOGI(network_tag, "Set WPA2 identity to %s", mywifi[ix].eap_identity);

      err = esp_wifi_sta_wpa2_ent_set_username((const unsigned char *)mywifi[ix].eap_identity,
        strlen(mywifi[ix].eap_identity));
      if (err != ESP_OK) {
        ESP_LOGE(network_tag, "Error %d setting WPA2 username, %s", err, esp_err_to_name(err));
	continue;
      } else
        ESP_LOGI(network_tag, "Set WPA2 username to %s", mywifi[ix].eap_identity);

      err = esp_wifi_sta_wpa2_ent_set_password((const unsigned char *)mywifi[ix].eap_password,
        strlen(mywifi[ix].eap_password));
      if (err != ESP_OK) {
        ESP_LOGE(network_tag, "Error %d setting WPA2 password, %s", err, esp_err_to_name(err));
	continue;
      } else
        ESP_LOGI(network_tag, "Set WPA2 password to %s", mywifi[ix].eap_password);

      esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
      err = esp_wifi_sta_wpa2_ent_enable(&config);
      if (err != ESP_OK) {
        ESP_LOGE(network_tag, "Error %d enabling Wifi with WPA2, %s", err, esp_err_to_name(err));
	continue;
      }
    } else {
      /*
       * This should be allowed anyway (use counter to limit attempts) : an example is a public
       * hotspot without password.
       */
      ESP_LOGI(network_tag, "Wifi %d, ssid [%s], WPA, no pwd", ix, mywifi[ix].ssid);
      // mywifi[ix].discard = true;
      continue;
    }

    /*
     * Set the Wifi to STAtion mode on the network specified by SSID (and optionally BSSID).
     */
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
      ESP_LOGE(network_tag, "Failed to set wifi mode to STA");		// FIXME
      return;
    }
    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (err != ESP_OK) {
      ESP_LOGE(network_tag, "Failed to set wifi config");		// FIXME
      return;
    }

    ESP_LOGI(network_tag, "Try wifi ssid [%s]", wifi_config.sta.ssid);
    err = esp_wifi_start();
    if (err != ESP_OK) {
      /*
       * FIX ME
       * usually the code survives this (ESP_OK happens) but an event gets fired into
       * wifi_event_handler().
       */
      ESP_LOGE(network_tag, "Failed to start wifi");			// FIXME
      return;
    }

    if (status == NS_SETUP_DONE) {
      /*
       * Note this doesn't say much ... real answer comes asynchronously
       */
      status = NS_CONNECTING;
      network = ix;
      return;
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
      last_connect = kippen->getCurrentTime();
      wifi_ok = false;

      SetupWifi();
#endif
      return;
    }
    status = NS_FAILED;
    last_connect = kippen->getCurrentTime();
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
  time_t now = kippen->getCurrentTime();
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
  ESP_LOGI(network_tag, "Starting WebServer");
  ws = new WebServer();
  if (acme) acme->setWebServer(ws->getServer());

  ESP_LOGI(network_tag, "Starting JSON Server");
  security->NetworkConnected(ctx, event);
}

void Network::NetworkDisconnected(void *ctx, system_event_t *event) {
  ESP_LOGE(network_tag, "Network disconnect ...");
  if (ws) {
    delete ws;
    ws = 0;
  }
  security->NetworkDisconnected(ctx, event);

#if 0
  ESP_LOGE(network_tag, "Disconnect : event id 0x%02x", event->event_id);

  if (event->event_id == SYSTEM_EVENT_STA_DISCONNECTED) {
    system_event_sta_disconnected_t *evp = &event->event_info.disconnected;

    char ssid[33];
    ssid[32] = 0;
    strncpy(ssid, (char *)evp->ssid, evp->ssid_len);
    if (evp->reason == WIFI_REASON_NO_AP_FOUND) {
      ESP_LOGE(network_tag, "Disconnect : ssid %s, reason 0x%02x (no access point)", ssid, evp->reason);
      status = NS_FAILED;
    } else
      ESP_LOGE(network_tag, "Disconnect : ssid %s, reason 0x%02x ", ssid, evp->reason);
  }
#endif
}


/*
 * Check whether the broadcast at startup to find peers was succesfull.
 */
void Network::loop(time_t now) {
  LoopRestartWifi(now);
}

bool Network::isConnected() {
  return (status == NS_RUNNING);
}

void Network::GotDisconnected(const char *fn, int line) {
  ESP_LOGE(network_tag, "Network is not connected (caller: %s line %d)", fn, line);
  status = NS_NONE;
}

void Network::Report() {
  ESP_LOGD(network_tag, "Report: status %s",
    (status == NS_RUNNING) ? "NS_RUNNING" :
    (status == NS_FAILED) ? "NS_FAILED" :
    (status == NS_NONE) ? "NS_NONE" :
    (status == NS_CONNECTING) ? "NS_CONNECTING" : "?");
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

  last_mqtt_message_received = kippen->getCurrentTime();
}

/*
 * (delayed) Restart handler
 */
void Network::ScheduleRestartWifi() {
  if (restart_time != 0) {
    // ESP_LOGE(network_tag, "%s: restart_time already set : %ld", __FUNCTION__, restart_time);
    return;
  }

  time_t now = kippen->getCurrentTime();
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

bool Network::NetworkIsNatted() {
  return mywifi[network].nat;
}

enum NetworkStatus Network::getStatus() {
  return status;
}

void Network::setStatus(enum NetworkStatus s) {
  status = s;
}

void Network::setReason(int r) {
  reason = r;
}

void Network::DiscardCurrentNetwork() {
  ESP_LOGE(network_tag, "Discarding network \"%s\"", mywifi[network].ssid);
  mywifi[network].discard = true;
}
