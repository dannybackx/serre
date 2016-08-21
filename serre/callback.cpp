/*
 * Handle the MQTT communication
 */

#define ENABLE_OTA
#define ENABLE_SNTP
#define	ENABLE_I2C

#include <ESP8266WiFi.h>
#include <Wire.h>       // Required to compile UPnP
#include "PubSubClient.h"
#include "SFE_BMP180.h"

extern "C" {
#include <sntp.h>
#include <time.h>
}

extern PubSubClient	client;

#include "personal.h"
#include "global.h"

void callback(char *topic, byte *payload, unsigned int length) {
  char *pl = (char *)payload;
  char reply[80];

  if (verbose & VERBOSE_CALLBACK) {
    Serial.print("Message arrived, topic [");
    Serial.print(topic);
    Serial.print("], payload {");
    for (int i=0; i<length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.printf(",%d}\n", length);
  }

  /*
  Serial.printf("Comparing strcmp(%s, %s) -> %d\n",
    topic, mqtt_topic_serre, strcmp(topic, mqtt_topic_serre));
  Serial.printf("Comparing strncmp(%s, %s, %d) -> %d\n",
    pl, mqtt_topic_boot_time, length, strncmp(pl, mqtt_topic_boot_time, length));
  /* */

  /*
   * Requests for information, or commands for the module
   */
  if (strcmp(topic, mqtt_topic_serre) == 0) {
    /*
     * Note : must always compare just the indicated length when comparing with payload.
     * Also make sure to compare in the right order, e.g.
     *   Serre/Valve/0
     * needs to be matched before
     *   Serre/Valve
     */
    if (strncmp(pl, mqtt_topic_bmp180, length) == 0) {
      /*
       * Read temperature / barometric pressure
       */
      if (verbose & VERBOSE_BMP) Serial.println("Topic bmp");

      if (bmp) {
        BMPQuery();

        int a, b, c;
        // Temperature
        a = (int) newTemperature;
        double td = newTemperature - a;
        b = 100 * td;
        c = newPressure;

        // Format the result
        sprintf(reply, "bmp (%2d.%02d, %d)", a, b, c);
  
      } else {
        sprintf(reply, "No sensor detected");
      }

      if (verbose & VERBOSE_BMP) Serial.println(reply);
      client.publish(mqtt_topic_bmp180, reply);
    } else if (strncmp(pl, mqtt_topic_valve, length) == 0) {
      /*
       * Read valve/pump status
       */
      if (verbose & VERBOSE_VALVE) Serial.println("Topic valve");
      client.publish(mqtt_topic_valve, valve ? "Open" : "Closed");
    } else if (strncmp(pl, mqtt_topic_valve_start, length) == 0) {
      if (verbose & VERBOSE_VALVE) Serial.println("Topic valve start");
    } else if (strncmp(pl, mqtt_topic_valve_stop, length) == 0) {
      if (verbose & VERBOSE_VALVE) Serial.println("Topic valve stop");
    } else if (strncmp(pl, mqtt_topic_restart, length) == 0) {
      // Reboot the ESP without a software update.
      ESP.restart();
    } else if (strncmp(pl, mqtt_topic_boot_time, length) == 0) {
      if (verbose & VERBOSE_SYSTEM) Serial.println("Topic boot time");

      now = localtime(&tsboot);
      strftime(reply, sizeof(reply), "Last booted on %F at %T", now);
      client.publish(mqtt_topic_boot_time, reply);

      if (verbose & VERBOSE_SYSTEM) Serial.printf("Reply {%s} {%s}\n", mqtt_topic_boot_time, reply);
    } else if (strncmp(pl, mqtt_topic_reconnects, length) == 0) {
      // Report the number of MQTT reconnects to our broker
      sprintf(reply, "Reconnect count %d", nreconnects);
      client.publish(mqtt_topic_reconnects, reply);
    }

    // End topic == mqtt_topic_serre

  } else if (strcmp(topic, mqtt_topic_bmp180) == 0) {
    // Ignore, this is a message we've sent out ourselves
  } else if (strcmp(topic, mqtt_topic_valve) == 0) {
    // Ignore, this is a message we've sent out ourselves
  } else {
    Serial.println("Unknown topic");
  }
}
