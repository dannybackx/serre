/*
 * Global class and variables for the chicken coop project
 *
 * Copyright (c) 2018, 2019, 2020 Danny Backx
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
#include <Wire.h>
#include "Ota.h"
#include "Secure.h"
#include "Acme.h"

#include <apps/sntp/sntp.h>
#include "mqtt_client.h"

extern String			ips, gws;

extern Ota			*ota;
extern Secure			*security;
extern Acme			*acme;


extern const char		*build;

class Kippen {
private:
  boolean mqttSubscribed;
  bool		sntp_up;

  esp_mqtt_client_config_t mqtt_config;
  esp_mqtt_client_handle_t mqtt;

  const char *reply_topic = "/kippen/reply";

public:
  bool Report(const char *msg);
  char *HandleQueryAuthenticated(const char *query, const char *caller);

  Kippen();

  void mqttReconnect();
  void mqttSubscribe();
  void NetworkConnected(void *ctx, system_event_t *event);
  void NetworkDisconnected(void *ctx, system_event_t *event);

  void HandleMqtt(char *topic, char *payload);

  boolean mqttConnected;
  time_t			nowts, boot_time;
  time_t 	getCurrentTime();
};

extern Kippen *kippen;
