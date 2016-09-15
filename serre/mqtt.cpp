/*
 * Copyright (c) 2016 Danny Backx
 *
 * License (MIT license):
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 */

#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include "ThingSpeak.h"

extern "C" {
#include <ip_addr.h>
#include <espconn.h>
}

#include "mywifi.h"
#include "global.h"

//
extern void callback(char * topic, byte *payload, unsigned int length);

const char *mqtt_user = MY_MQTT_USER;
const char *mqtt_password = MY_MQTT_PASSWORD;

// One "client" per application / connection
extern WiFiClient	pubSubEspClient, IfTttEspClient, TSEspClient;
extern PubSubClient	client;
void ext_mqtt_connect_gethostbyname(const char *server);
ip_addr_t *mqtt_ext_server;		// IP Address of MQTT server, if on ext network
int mqtt_connect_reason = 1;	// espconn.h : 0 is ok, errors are negative

/*
 * There used to be a loop inside this function.
 * Removed that, we're getting called from the standard Arduino loop() anyway.
 */
void mqtt_reconnect(void) {
  if (mqtt_connect_reason > 0) {
    // We're not in a reconnect cycle yet, so talk, and do DNS lookup if required.
    if (external_network) {
      // This will asynchronously call client.setServer() as well.
      ext_mqtt_connect_gethostbyname(MQTT_EXT_SERVER);
      Serial.println("Starting MQTT (external network)");
    } else {
      client.setServer(mqtt_server, mqtt_port);
      Serial.println("Starting MQTT");
    }
    client.setCallback(callback);
  }

  // Attempt to connect
  if (client.connect(MQTT_HOSTNAME, mqtt_user, mqtt_password)) {
    nreconnects++;

    if (external_network)
      Serial.printf("MQTT Connected (%d.%d.%d.%d)\n",
	ip4_addr1(mqtt_ext_server), ip4_addr2(mqtt_ext_server),
	ip4_addr3(mqtt_ext_server), ip4_addr4(mqtt_ext_server));
    else
      Serial.println("MQTT Connected");

    // Get the current time
    tsnow = et->now(NULL);
    now = localtime(&tsnow);

    // Once connected, publish an announcement...
    if (mqtt_initial) {
      strftime(buffer, sizeof(buffer), "boot %F %T", now);
      client.publish(mqtt_topic, buffer);

      mqtt_initial = 0;
    } else {
      strftime(buffer, sizeof(buffer), "reconnect %F %T", now);
      client.publish(mqtt_topic, buffer);
    }

    // ... and (re)subscribe
    client.subscribe(mqtt_topic);
    client.subscribe(mqtt_topic_valve);
    client.subscribe(mqtt_topic_bmp180);
    client.subscribe(mqtt_topic_verbose);
    client.subscribe(mqtt_topic_schedule);
    client.subscribe(mqtt_topic_schedule_set);
  }
}

static void _dns_found_cb(const char *name, ip_addr_t *ipaddr, void *arg)
{
  // Serial.printf("MQTT connect to %d.%d.%d.%d\n",
  //   ip4_addr1(ipaddr), ip4_addr2(ipaddr), ip4_addr3(ipaddr), ip4_addr4(ipaddr));
  client.setServer((uint8_t *)ipaddr, MQTT_EXT_PORT);
  mqtt_ext_server = ipaddr;
}

void ext_mqtt_connect_gethostbyname(const char *server)
{
   ip_addr_t ip_addr;

   mqtt_connect_reason = espconn_gethostbyname(NULL, server, &ip_addr, _dns_found_cb);

   switch (mqtt_connect_reason) {
   case ESPCONN_INPROGRESS:
      // Serial.println("Lookup in progress");
      break;

   case ESPCONN_OK:
      // Serial.println("Lookup ok");
      _dns_found_cb(server, &ip_addr, NULL);
      break;
      
   case ESPCONN_ARG:
      Serial.println("Lookup arg error");
      break;
      
   default:   
      Serial.println("Lookup default");
      break;
   }
}

const char *mqtt_topic =		SystemId;
const char *mqtt_topic_bmp180 =		SystemId "/BMP180";
const char *mqtt_topic_valve =		SystemId "/Valve";
const char *mqtt_topic_valve_start =	SystemId "/Valve/1";
const char *mqtt_topic_valve_stop =	SystemId "/Valve/0";
const char *mqtt_topic_report =		SystemId "/Report";
const char *mqtt_topic_report_freq =	SystemId "/Report/Frequency";
const char *mqtt_topic_restart =	SystemId "/System/Reboot";
const char *mqtt_topic_reconnects =	SystemId "/System/Reconnects";
const char *mqtt_topic_boot_time =	SystemId "/System/Boot";
const char *mqtt_topic_current_time =	SystemId "/System/Time";
const char *mqtt_topic_schedule_set =	SystemId "/Schedule/Set";
const char *mqtt_topic_schedule =	SystemId "/Schedule";
const char *mqtt_topic_verbose =	SystemId "/System/Verbose";
const char *mqtt_topic_version =	SystemId "/System/Version";
const char *mqtt_topic_info =		SystemId "/System/Info";
