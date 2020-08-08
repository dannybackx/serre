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

#ifndef	_MY_NETWORK_H_
#define	_MY_NETWORK_H_

#include <Arduino.h>
#include <esp_event_loop.h>
#include "WebServer.h"

enum NetworkStatus {
  NS_NONE,		// Network uninitialized
  NS_SETUP_DONE,	// OS/HW init calls were performed by the app
  NS_CONNECTING,	// Wifi connected, awaiting IP address
  NS_RUNNING,		// We're alive and kicking
  NS_FAILED		// We got completely disconnected
};

class Network {
public:
  void mqttConnected();
  void mqttDisconnected();
  void mqttSubscribed();
  void mqttUnsubscribed();
  void gotMqttMessage();

  // Indicate failure
  void disconnected(const char *fn, int line);
  void GotDisconnected(const char *fn, int line);
  void eventDisconnected(const char *fn, int line);

  void SetupWifi(void);
  void WaitForWifi(void);
  void setWifiOk(boolean);

  Network();
  ~Network();

  void loop(time_t now);

  bool isConnected();

  void StartKeepAlive(time_t);
  void ReceiveKeepAlive();

  void Report();

  // Restart
  void ScheduleRestartWifi();
  void StopWifi();
  void RestartWifi();

  void NetworkConnected(void *ctx, system_event_t *event);
  void NetworkDisconnected(void *ctx, system_event_t *event);

  bool NetworkIsNatted();
  void setStatus(enum NetworkStatus);
  enum NetworkStatus getStatus();
  void setReason(int);
  void DiscardCurrentNetwork();

private:
  const char		*network_tag = "Network";
  bool			wifi_ok;
  enum NetworkStatus	status;
  int			reason;
  int			network;

  time_t		last_connect;
  int			reconnect_interval;

  // MQTT
  time_t		last_mqtt_message_received;
  uint			mqtt_message;

  // Restart
  time_t		restart_time;
  void LoopRestartWifi(time_t now);
};

// Global variables
extern Network *network;
extern WebServer *ws;
#endif
