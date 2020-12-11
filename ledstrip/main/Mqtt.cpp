/*
 * MQTT server
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

#include <Arduino.h>	// For IPAddress, ..
#include <time.h>

#include <list>
using namespace std;

#include "App.h"
#include "Mqtt.h"
#include "secrets.h"
#include "StableTime.h"
#include "Network.h"

#include "mqtt_client.h"
#include <freertos/task.h>
#include <sys/socket.h>
#include "esp_wifi.h"


// These used to be in the class, but need to be global for the background task
const char *mqtt_tag = "MQTT";

// For MQTT
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
esp_mqtt_client_config_t mqtt_config;
esp_mqtt_client_handle_t mqtth;

boolean mqtt_signal = false;

// Forward
esp_err_t PeersNetworkConnected(void *ctx, system_event_t *event);
esp_err_t PeersNetworkDisconnected(void *ctx, system_event_t *event);

/********************************************************************************
 *
 * Ctor, dtor, network connect/disconnect handlers
 *
 *******************************************************************************/
Mqtt::Mqtt() {

  mqttConnected = false;
  mqttSubscribed = false;

  mqtt_signal = false;

  network->RegisterModule(mqtt_tag, PeersNetworkConnected, PeersNetworkDisconnected);
}

Mqtt::~Mqtt() {
}

esp_err_t PeersNetworkConnected(void *ctx, system_event_t *event) {
  ESP_LOGI(mqtt->mqtt_tag, "Network Connected, ip %s",
    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

  mqtt->local = event->event_info.got_ip.ip_info.ip;
  mqtt->unconnected = -1;

  ESP_LOGD(mqtt->mqtt_tag, "Initializing MQTT");
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = MQTT_URI;
  mqtt_config.event_handle = mqtt_event_handler;

  // Note Tuan's MQTT component starts a separate task for event handling
  mqtth = esp_mqtt_client_init(&mqtt_config);
  esp_err_t err = esp_mqtt_client_start(mqtth);

  if (err == ESP_OK)
    ESP_LOGD(mqtt->mqtt_tag, "MQTT Client Start ok");
  else
    ESP_LOGE(mqtt->mqtt_tag, "MQTT Client Start failure : %d", err);

  mqtt->GetWifiInfo();

  // Don't do anything here
  return ESP_OK;
}

esp_err_t PeersNetworkDisconnected(void *ctx, system_event_t *event) {
  ESP_LOGI(mqtt->mqtt_tag, "Network disconnected");
  if (mqtt) {
    esp_err_t err = esp_mqtt_client_stop(mqtth);
    ESP_LOGD(mqtt->mqtt_tag, "MQTT Client Stop : %d %s", (int)err,
      (err == ESP_OK) ? "ok" : (err == ESP_FAIL) ? "fail" : "?");
  }
  ESP_LOGD(mqtt->mqtt_tag, "Network disconnected --> end of function");
  
  return ESP_OK;
}

/*
 * Report environmental information periodically
 */
void Mqtt::loop(time_t nowts) {
  if (network && network->isConnected()) {
    // Maybe still too early ...
    if (p2psock < 0 || mcsock < 0)
      return;

  }
}

/*********************************************************************************
 *
 * MQTT
 *
 * This function is the event handler : the nuts and bolts to work with this
 * particular MQTT implementation.
 *
 * Tuan's MQTT component starts a separate task to handle this.
 *
 *********************************************************************************/
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
  char topic[80], message[80];				// FIX ME limitation
  void HandleMqtt(char *topic, char *payload);
  static time_t conn_ts = 0L;

  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(mqtt_tag, "mqtt connected");
    network->mqttConnected();
    conn_ts = esp_timer_get_time();
    if (mqtt) {
      mqtt->mqttConnected = true;
      mqtt->mqttSubscribe();
    } else {
      // This should not happen, there's almost nothing after mqtt startup in our ctor
      ESP_LOGE(mqtt_tag, "MQTT : no peers, couldn't connect");
    }
    // mqtt_signal = true;			// FIXME
    break;
  case MQTT_EVENT_DISCONNECTED:
    // Track how soon this happens after connect
    {
      time_t disc_ts = esp_timer_get_time();
      ESP_LOGE(mqtt_tag, "DISCONNECT : conn %ld disc %ld, diff %ld", conn_ts, disc_ts, disc_ts - conn_ts);
      if (disc_ts > conn_ts && (disc_ts - conn_ts < 1000000L)) {
        ESP_LOGE(mqtt_tag, "Ignoring early disconnect event");
        return ESP_OK;
      }
    }
    ESP_LOGE(mqtt_tag, "mqtt disconnected");
    if (mqtt)
      mqtt->mqttConnected = false;
    network->mqttDisconnected();
    mqtt_signal = false;
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGD(mqtt_tag, "mqtt subscribed");
    network->mqttSubscribed();
    if (mqtt)
      mqtt->mqttSubscribed = true;
    // mqtt_signal = true;			// FIXME
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGE(mqtt_tag, "mqtt subscribed");
    network->mqttUnsubscribed();
    if (mqtt)
      mqtt->mqttSubscribed = false;
    // mqtt_signal = false;
    break;
  case MQTT_EVENT_PUBLISHED:
    break;
  case MQTT_EVENT_DATA:
    // Ignore our own replies
    if (strncmp(topic, mqtt->reply_topic, strlen(mqtt->reply_topic)) == 0)
      return ESP_OK;

    // Indicate that we just got a message so we're still alive
    network->gotMqttMessage();

    ESP_LOGD(mqtt_tag, "MQTT topic %.*s message %.*s",
      event->topic_len, event->topic, event->data_len, event->data);

    // Make safe copies, then call business logic handler
    strncpy(topic, event->topic, event->topic_len);
    topic[(event->topic_len > 79) ? 79 : event->topic_len] = 0;
    strncpy(message, event->data, event->data_len);
    message[(event->data_len > 79) ? 79 : event->data_len] = 0;

    // Handle it already
    HandleMqtt(topic, message);

    ESP_LOGD(mqtt_tag, "MQTT end handler");

    mqtt_signal = true;			// FIXME
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGD(mqtt_tag, "mqtt event error");
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    ESP_LOGD(mqtt_tag, "mqtt event before connect");
    break;
  }

  return ESP_OK;
}

/*
 * MQTT : Handle business logic
 *
 * Handle most payloads with either
 *	/alarm
 * or	/alarm/node/{my-node-name}
 *
 * Messages with topic /alarm/reply are ignored (that's the purpose of the reply subtopic).
 */
void HandleMqtt(char *topic, char *payload) {
  extern void mqttMyNodeCallback(char *);

  ESP_LOGI(mqtt_tag, "HandleMqtt(%s,%s)", topic, payload);

  mqttMyNodeCallback(payload);
}

static void PeersRebootHandler(const char *payload) {
  char reply[80];
  esp_mqtt_client_publish(mqtth, mqtt->reply_topic, reply, 0, 0, 0);
  esp_restart();
}

static void PeersOtaHandler(const char *payload) {
  ESP_LOGE(mqtt_tag, "MQTT OTA");
  ota->DoOTA();
}

void timeString(time_t t, const char *format, char *buffer, int len) {
  struct tm *tmp = localtime(&t);
  strftime(buffer, len, format, tmp);
}

static void PeersBootHandler(const char *payload) {
  char reply[80];
  char msg[60], format[40];
  sprintf(format, "Boot %s %%F %%R", "ledstrip");
  timeString(boot_time, format, msg, sizeof(msg));
  sprintf(reply, "Boot %s %s", "ledstrip", msg);
  esp_mqtt_client_publish(mqtth, mqtt->reply_topic, reply, 0, 0, 0);
}

static void PeersTimeHandler(const char *payload) {
    char msg[64], format[48];
    sprintf(format, "Time %s %%F %%R", "ledstrip");
    timeString(time(0), format, msg, sizeof(msg));
    esp_mqtt_client_publish(mqtth, mqtt->reply_topic, msg, 0, 0, 0);
}

static void PeersNetworkHandler(const char *payload) {
  tcpip_adapter_ip_info_t ipInfo;
  wifi_ap_record_t apinfo;

  esp_err_t err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
  if (err == ESP_OK) {
    char ip[16];
    ip4addr_ntoa_r(&ipInfo.ip, ip, sizeof(ip));

    err = esp_wifi_sta_get_ap_info(&apinfo);

    if (err == ESP_OK) {
      char reply[80];
      sprintf(reply,
	"Alarm node %s, ip %s, mac %02x:%02x:%02x:%02x:%02x:%02x, ap %s ch %d rssi %d",
	"ledstrip", ip,
	apinfo.bssid[0], apinfo.bssid[1], apinfo.bssid[2], apinfo.bssid[3], apinfo.bssid[4],
	apinfo.bssid[5], apinfo.ssid, apinfo.primary, apinfo.rssi);

      esp_mqtt_client_publish(mqtth, mqtt->reply_topic, reply, 0, 0, 0);
    }
  }

  if (err != ESP_OK)
    ESP_LOGE(mqtt_tag, "Could not get network info");
}

void PeersWifiHandler(const char *payload) {
  mqtt->GetWifiInfo();
}

struct payload_handler_table {
  const char *pl;
  uint8_t len;
  void (*h)(const char *);
} pht[] = {
  { "reboot",		0,	PeersRebootHandler },
  { "ota",		0,	PeersOtaHandler },
  { "boot",		0,	PeersBootHandler },
  { "time",		0,	PeersTimeHandler },
  { "network",		0,	PeersNetworkHandler },
  { NULL,		0,	NULL }
};
/*
 * Process commands for this node only
 * Note : payload is null-terminated here.
 *
 * Payloads handled :
 *	QUERY			ACTION
 *	boot			reboot
 *	title			ota
 *	time
 *	query			arm
 *	build			night
 *	network			disarm
 *	/peers/query
 *	arp
 *	config		--> crash, memory allocation too tight ?	FIXME
 *
 *	DEBUG (output only on console, not on MQTT)
 *	wifi
 *	listalive
 *	listnodes
 *	update_timeout
 *	heap
 */
void mqttMyNodeCallback(char *payload) {
  ESP_LOGD(mqtt_tag, "mqttMyNodeCallback(%s)", payload);

  for (int i=0; pht[i].pl; i++) {
    if (pht[i].len && strncmp(pht[i].pl, payload, pht[i].len) == 0) {
      ESP_LOGE(mqtt_tag, "Handler %d %s called", i, payload);
      pht[i].h(payload);
      return;
    } else if (strcmp(pht[i].pl, payload) == 0) {
      ESP_LOGE(mqtt_tag, "Handler %d %s called", i, payload);
      pht[i].h(payload);
      return;
    }
  }
}

void Mqtt::mqttReconnect() {
  ESP_LOGI(mqtt_tag, "Initializing MQTT");
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = MQTT_URI;
  mqtt_config.event_handle = mqtt_event_handler;

  // Note Tuan's MQTT component starts a separate task for event handling
  // mqtt = esp_mqtt_client_init(&mqtt_config);
  esp_err_t err = esp_mqtt_client_start(mqtth);

  ESP_LOGD(mqtt_tag, "MQTT Client Start : %d %s", (int)err,
    (err == ESP_OK) ? "ok" : (err == ESP_FAIL) ? "fail" : "?");
}

void Mqtt::mqttSubscribe() {
  if (mqttConnected) {
    esp_mqtt_client_subscribe(mqtth, topic_wildcard, 0);
  }
}

bool Mqtt::Report(const char *msg) {
  if (mqttConnected) {
    esp_mqtt_client_publish(mqtth, reply_topic, msg, 0, 0, 0);
    return true;
  }

  ESP_LOGD(mqtt_tag, "Report: mqtt not connected yet (msg %s)", msg);
  return false;
}


bool Mqtt::isMqttConnected() {
  return mqtt->mqttConnected;
}

void Mqtt::GetWifiInfo() {
  wifi_ap_record_t apinfo;
  esp_err_t err;

  err = esp_wifi_sta_get_ap_info(&apinfo);
  if (err) {
    ESP_LOGE(mqtt_tag, "get_ap_info failed");
    return;
  }

  ESP_LOGI(mqtt_tag, "AP info : %s (%02x:%02x:%02x:%02x:%02x:%02x, ch %d) rssi %d", apinfo.ssid,
  	apinfo.bssid[0], apinfo.bssid[1], apinfo.bssid[2],
	apinfo.bssid[3], apinfo.bssid[4], apinfo.bssid[5],
  	apinfo.primary, apinfo.rssi);
}
