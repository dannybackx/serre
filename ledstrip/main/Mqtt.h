/*
 * MQTT module
 *
 * Copyright (c) 2017, 2018, 2019 Danny Backx
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

#ifndef	_PEER_H_
#define	_PEER_H_

#include <esp_event.h>

#include <list>
using namespace std;

class Mqtt {
public:
  Mqtt();
  ~Mqtt();
  void loop(time_t);

  bool Report(const char *msg);
  void timeString(time_t t, const char *format, char *buffer, int len);

  void mqttSubscribe();
  void mqttReconnect();
  bool	 mqttConnected, mqttSubscribed;

  friend void PeersWifiHandler(const char *payload);
  friend void PeersNodesHandler(const char *payload);
  friend void HandleMqtt(char *topic, char *payload);
  friend void mqttMyNodeCallback(char *payload);
  friend void ImageTaskLoop(void *ptr);

  friend esp_err_t PeersNetworkConnected(void *ctx, system_event_t *event);
  friend esp_err_t PeersNetworkDisconnected(void *ctx, system_event_t *event);

  bool isMqttConnected();

  // Allocate this only once
  const char *reply_topic = "/ledstrip/reply";
  const char *my_topic = "/ledstrip";
  const char *topic_wildcard = "/ledstrip/#";

private:

  int		p2psock, mcsock;
  uint		mc_messages;			// Number of warning messages about mcsock
  uint		p2p_accept_count;
  time_t	unconnected;
  const uint16_t portMulti = 23456;		// port number used for all our communication
  uint8_t	 packetBuffer[512];		// buffer to hold incoming and outgoing packets
  IPAddress ipMulti;
  ip4_addr_t	local;

  const int recBufLen = 512;

  char output[256];

  const char *mqtt_tag = "Mqtt";

  void GetWifiInfo();

};

extern Mqtt *mqtt;
#endif	/* _PEER_H_ */
