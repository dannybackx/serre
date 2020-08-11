/*
 * Main program to manage the door to a chicken coop.
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

#include <Arduino.h>
#include "Kippen.h"
#include <Wire.h>
#include "Ota.h"
#include "Network.h"
#include "Secure.h"
#include "Acme.h"
#include "Dyndns.h"
#include "Temperature.h"
#include "SimpleL298.h"

#include <esp_littlefs.h>

static const char *kippen_tag = "kippen";
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);

Kippen		*kippen = 0;
Ota		*ota = 0;
Network		*network = 0;
Secure		*security = 0;
Acme		*acme = 0;
Dyndns		*dyndns = 0;
Temperature	*temperature = 0;
SimpleL298	*simple = 0;

time_t		dyndns_last = 0;
bool		ftp_started = false;

// Initial function
void setup(void) {
  Serial.begin(115200);

  delay(250);

  kippen = new Kippen();

  ESP_LOGI(kippen_tag, "Controller (c) 2017, 2018, 2019, 2020 by Danny Backx");

  extern const char *build;
  ESP_LOGI(kippen_tag, "Build timestamp %s", build);

  ESP_LOGD(kippen_tag, "Free heap : %d", ESP.getFreeHeap());

  /* Network */
  network = new Network();
  security = new Secure();	// Needs to be active before config

  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  ESP_LOGI(kippen_tag, "ESP32 chip with %d CPU cores, WiFi%s%s, silicon revision %d",
    chip_info.cores,
    (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
    (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
    chip_info.revision);

#if defined(IDF_MAJOR_VERSION)
  ESP_LOGI(kippen_tag, "IDF version %s (build v%d.%d)",
      esp_get_idf_version(), IDF_MAJOR_VERSION, IDF_MINOR_VERSION);
#elif defined(IDF_VER)
  ESP_LOGI(kippen_tag, "IDF version %s (build %s)", esp_get_idf_version(), IDF_VER);
#else
  ESP_LOGI(kippen_tag, "IDF version %s (build version unknown)", esp_get_idf_version());
#endif

  // Set log levels FIXME
  // Make stuff from the underlying libraries quieter
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_ERROR);
  esp_log_level_set("wifi", ESP_LOG_ERROR);
  esp_log_level_set("system_api", ESP_LOG_ERROR);
  esp_log_level_set("Acme", ESP_LOG_DEBUG);
  esp_log_level_set("esp_littlefs", ESP_LOG_ERROR);

  network->SetupWifi();

#ifdef CONFIG_USE_LITTLEFS
    // Configure file system access
    esp_vfs_littlefs_conf_t lcfg;
    lcfg.base_path = CONFIG_FS_BASEDIR;
    lcfg.partition_label = "spiffs";
    lcfg.format_if_mount_failed = true;
    esp_err_t err = esp_vfs_littlefs_register(&lcfg);
    if (err != ESP_OK) {
      ESP_LOGE(kippen_tag, "Failed to register LittleFS %s (%d)", esp_err_to_name(err), err);
    } else {
      ESP_LOGI(kippen_tag, "Registered LittleFS %s", CONFIG_FS_BASEDIR);
    }
#else
    ESP_LOGE(kippen_tag, "No filesystem defined");
#endif

  /*
   * Set up the time
   *
   * See https://www.di-mgt.com.au/wclock/help/wclo_tzexplain.html for examples of TZ strings.
   * This one works for Europe : CET-1CEST,M3.5.0/2,M10.5.0/3
   * I assume that this one would work for the US : EST5EDT,M3.2.0/2,M11.1.0
   */
  setenv("TZ", CONFIG_TIMEZONE, 1);

  char *msg = (char *)malloc(180), s[32];
  msg[0] = 0;

  if (CONFIG_WEBSERVER_PORT != -1) {
    sprintf(s, " webserver(%d)", CONFIG_WEBSERVER_PORT);
    strcat(msg, s);
  }
  if (CONFIG_JSON_SERVERPORT > 0) {
    sprintf(s, " JSON(%d)", CONFIG_JSON_SERVERPORT);
    strcat(msg, s);
#ifdef CONFIG_RUN_ACME
    sprintf(s, " ACME");
    strcat(msg, s);
#endif
  }

  sprintf(s, " FTP");
  strcat(msg, s);

#ifdef CONFIG_DYNDNS_ENABLED
    sprintf(s, " DynDNS");
    strcat(msg, s);
#endif

  // i2c
#if (defined(CONFIG_I2C_SDA_PIN) && defined(CONFIG_I2C_SCL_PIN))
    ESP_LOGD(kippen_tag, "Initialize i2c (sda %d, scl %d)\n", CONFIG_I2C_SDA_PIN,
      CONFIG_I2C_SCL_PIN);
    sprintf(s, " i²c (sda %d scl %d)", CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN);
    strcat(msg, s);
    Wire.begin(CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN);	// on the PCB : 26, 27

  // Local temperature sensors - requires I²C
  temperature = new Temperature();

  sprintf(s, " temp");
  strcat(msg, s);
#endif

  ESP_LOGI(kippen_tag, "Kippen controller, have :%s ", msg);
  free(msg);
  msg = 0;

  ota = new Ota();

  ESP_LOGI(kippen_tag, "FS prefix %s", CONFIG_FS_BASEDIR);

#if defined(CONFIG_ACME_ENABLED) && defined(CONFIG_USE_LITTLEFS)
    acme = new Acme();
    acme->setFilenamePrefix(CONFIG_FS_BASEDIR);

  // Do ACME if we have a secure server, and we require it
  if (CONFIG_JSON_SERVERPORT > 0) {

    acme->setUrl(CONFIG_ACME_DOMAIN);
    acme->setAcmeServer(CONFIG_ACME_SERVER_URL);
    acme->setEmail(CONFIG_ACME_EMAIL);
#if 0
    acme->setFtpServer(CONFIG_FTP_WEBSERVER_IP);
    acme->setFtpPath(CONFIG_FTP_WEBSERVER_PATH);
    acme->setFtpUser(CONFIG_FTP_WEBSERVER_FTPUSER);
    acme->setFtpPassword(CONFIG_FTP_WEBSERVER_FTPPASS);
#endif
    acme->setAccountFilename(CONFIG_ACME_ACCOUNT_FN);
    acme->setOrderFilename(CONFIG_ACME_ORDER_FN);
    acme->setAccountKeyFilename(CONFIG_ACME_ACCOUNT_KEY_FN);
    acme->setCertKeyFilename(CONFIG_CERT_KEY_FN);
    acme->setCertificateFilename(CONFIG_ACME_CERTIFICATE_FN);

    ESP_LOGI("Acme", "URL %s", CONFIG_ACME_DOMAIN);
    ESP_LOGI("Acme", "Server %s", CONFIG_ACME_SERVER_URL);
    ESP_LOGI("Acme", "Email %s", CONFIG_ACME_EMAIL);

    ESP_LOGI("Acme", "Account fn %s", CONFIG_ACME_ACCOUNT_FN);
    ESP_LOGI("Acme", "Order fn %s", CONFIG_ACME_ORDER_FN);
    ESP_LOGI("Acme", "Account key fn %s", CONFIG_ACME_ACCOUNT_KEY_FN);
    ESP_LOGI("Acme", "Certificate key fn %s", CONFIG_CERT_KEY_FN);
    ESP_LOGI("Acme", "Certificate fn %s", CONFIG_ACME_CERTIFICATE_FN);

    if (! acme->HaveValidCertificate()) {
      /*
       * This is the stuff that can happen even when not connected
       */
      if (acme->getAccountKey() == 0) {
        acme->GenerateAccountKey();
      }
      if (acme->getCertificateKey() == 0) {
        acme->GenerateCertificateKey();
      }

      ESP_LOGI(kippen_tag, "Don't have a valid certificate ...");
      // acme->CreateNewAccount();
      // acme->CreateNewOrder();
    } else {
      ESP_LOGI(kippen_tag, "Certificate is valid");
    }
  }
#endif

#ifdef CONFIG_DYNDNS_ENABLED
    dyndns = new Dyndns(DD_NOIP);
    if (CONFIG_DYNDNS_URL == 0 || CONFIG_DYNDNS_AUTH == 0) {
      // We need both the URL to keep alive, and authentication data.
      ESP_LOGE(kippen_tag, "Can't run DynDNS - insufficient configuration");
    } else {
      dyndns->setHostname(CONFIG_DYNDNS_URL);
      ESP_LOGI(kippen_tag, "Running DynDNS for domain %s", CONFIG_DYNDNS_URL);
      dyndns->setAuth(CONFIG_DYNDNS_AUTH);
      ESP_LOGI(kippen_tag, "DynDNS auth %s", CONFIG_DYNDNS_AUTH);
    }
#endif

  network->WaitForWifi();

  simple = new SimpleL298(CONFIG_L298_CHANNEL_A_DIR1_PIN,	// 16
  			CONFIG_L298_CHANNEL_A_DIR2_PIN,		// 23
			CONFIG_L298_CHANNEL_A_SPEED_PIN);	// 17
}

void loop()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  kippen->nowts = tv.tv_sec;

  // Record boot time
  if (kippen->boot_time == 0 && kippen->nowts > 1000) {
    kippen->boot_time = kippen->nowts;

    char msg[80], ts[24];
    struct tm *tmp = localtime(&kippen->boot_time);
    strftime(ts, sizeof(ts), "%Y-%m-%d %T", tmp);
    sprintf(msg, "Kippen controller boot at %s", ts);

    if (!kippen->Report(msg)) {
      ESP_LOGE(kippen_tag, "Could not report boot time");
    }
  }

  if (kippen->nowts > 1000 && ! ftp_started) {
    extern void ftp_init();
    ftp_init();
    ftp_started = true;
  }

  if (network) network->loop(kippen->nowts);
  if (security) security->loop(kippen->nowts);

  // Weekly DynDNS update (1w = 86400s)
  if (dyndns && (kippen->nowts > 1000000L)) {
    if ((dyndns_last == 0) || (((kippen->nowts - dyndns_last) / 1000000) > 86)) {
      if (dyndns->update()) {
        ESP_LOGI(kippen_tag, "DynDNS update succeeded");
	dyndns_last = kippen->nowts;
      } else
	ESP_LOGE(kippen_tag, "DynDNS update failed");
    }
  }

  // ACME : only run if not NATted
  if (acme && ! network->NetworkIsNatted()) {
    acme->loop(kippen->nowts);
  }

  // Temperature
  if (temperature)
    temperature->loop(kippen->nowts);
}

extern "C" {
  int heap() {
    return ESP.getFreeHeap();
  }

  /*
   * Arduino startup code, if you build with ESP-IDF without the startup code enabled.
   */
  void app_main() {
    initArduino();

    Serial.begin(115200);
    Serial.printf("Yow ... starting sketch\n");

    setup();
    while (1)
      loop();
  }
}

bool Kippen::Report(const char *msg) {
  if (mqttConnected) {
    ESP_LOGD(kippen_tag, "MQTT report msg %s", msg);

    esp_mqtt_client_publish(mqtt, reply_topic, msg, 0, 0, 0);
    return true;
  }

  ESP_LOGE(kippen_tag, "Report: mqtt not connected yet (msg %s)", msg);
  return false;
}

void Kippen::NetworkConnected(void *ctx, system_event_t *event) {
  ESP_LOGD(kippen_tag, "Network connected, ip %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

  if (! sntp_up)
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_up = true;
  sntp_init();
  sntp_setservername(0, (char *)NTP_SERVER_0);
  sntp_setservername(1, (char *)NTP_SERVER_1);


  ESP_LOGI(kippen_tag, "Initializing MQTT");
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = MQTT_URI;
  mqtt_config.event_handle = mqtt_event_handler;

  // Note Tuan's MQTT component starts a separate task for event handling
  mqtt = esp_mqtt_client_init(&mqtt_config);
  esp_err_t err = esp_mqtt_client_start(mqtt);

  if (err == ESP_OK)
    ESP_LOGI(kippen_tag, "MQTT Client Start ok");
  else
    ESP_LOGE(kippen_tag, "MQTT Client Start failure : %d", err);
}

void Kippen::NetworkDisconnected(void *ctx, system_event_t *event) {
  sntp_stop();
  sntp_up = false;
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
  char topic[80], message[80];				// FIX ME limitation
  void HandleMqtt(char *topic, char *payload);

  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(kippen_tag, "mqtt connected");
    network->mqttConnected();
    kippen->mqttConnected = true;
    kippen->mqttSubscribe();
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGE(kippen_tag, "mqtt disconnected");
    network->mqttDisconnected();
    kippen->mqttConnected = false;
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGD(kippen_tag, "mqtt subscribed");
    network->mqttSubscribed();
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGE(kippen_tag, "mqtt unsubscribed");
    network->mqttUnsubscribed();
    break;
  case MQTT_EVENT_PUBLISHED:
    break;
  case MQTT_EVENT_DATA:
    // No need to ignore own replies if we don't wildcard-subscribe
    // Ignore our own replies
    // if (strncmp(topic, "/kippen/reply", 12) == 0)
    //   return ESP_OK;

    // Indicate that we just got a message so we're still alive
    network->gotMqttMessage();

    ESP_LOGI(kippen_tag, "MQTT topic %.*s message %.*s",
      event->topic_len, event->topic, event->data_len, event->data);

    // Make safe copies, then call business logic handler
    strncpy(topic, event->topic, event->topic_len);
    topic[(event->topic_len > 79) ? 79 : event->topic_len] = 0;
    strncpy(message, event->data, event->data_len);
    message[(event->data_len > 79) ? 79 : event->data_len] = 0;

    // Handle it already
    kippen->HandleMqtt(topic, message);

    ESP_LOGD(kippen_tag, "MQTT end handler");

    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGD(kippen_tag, "mqtt event error");
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    ESP_LOGD(kippen_tag, "mqtt event before connect");
    break;
  }

  return ESP_OK;
}

void Kippen::mqttReconnect() {
  ESP_LOGI(kippen_tag, "Initializing MQTT");
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = MQTT_URI;
  mqtt_config.event_handle = mqtt_event_handler;

  // Note Tuan's MQTT component starts a separate task for event handling
  // mqtt = esp_mqtt_client_init(&mqtt_config);
  esp_err_t err = esp_mqtt_client_start(mqtt);

  ESP_LOGD(kippen_tag, "MQTT Client Start : %d %s", (int)err,
    (err == ESP_OK) ? "ok" : (err == ESP_FAIL) ? "fail" : "?");
}

/*
 * Note : don't wildcard-subscribe, this means you don't have to filter away own replies in 
 * mqtt_event_handler():MQTT_EVENT_DATA
 */
void Kippen::mqttSubscribe() {
  if (mqttConnected) {
    esp_mqtt_client_subscribe(mqtt, "/kippen", 0);	// Note no wildcard "/kippen/#"
  }
}

Kippen::Kippen() {
  mqttConnected = false;
  mqttSubscribed = false;
  nowts = boot_time = 0;
  sntp_up = false;
}

char *Kippen::HandleQueryAuthenticated(const char *query, const char *caller) {
  return 0;
}

void Kippen::HandleMqtt(char *topic, char *payload) {
}

time_t Kippen::getCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec;
}
