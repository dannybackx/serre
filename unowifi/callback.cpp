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

/*
 * Handle the MQTT communication
 */

#include <Arduino.h>
#include <ArduinoWiFi.h>
#include <Wire.h>
#include "SFE_BMP180.h"
#include "global.h"
//#include "buildinfo.h"
#include <TimeLib.h>
#include <DS1307RTC.h>

char reply[20];

#if 0
void ProcessCallback() {
// void ProcessCallback(String command, WifiData client) {
  // String command = client.readString();
  String command = Wifi.readString();
  const char *p = command.c_str();

  Serial.print(" {");
  Serial.print(p);
  Serial.println("}");

  if (command == mqtt_topic_bmp180) {
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
    Wifi.print(reply);
  // Add cases here
  } else if (command.startsWith("/arduino/digital/time/set/")) {
    Serial.println("Yes ! Got RTC time set command");
  } else if (strncmp(p, "/arduino/digital/time/set/", strlen("/arduino/digital/time/set/")) == 0) {
    const char *q = p + strlen("/arduino/digital/time/set/");
    if (q[0] == 'T') {
      int t = atoi(q+1);
      RTC.set(t);
      Serial.println("Got RTC time set command");
    } else {
      Serial.print("Invalid time code ");
      Serial.print(q);
      Serial.println("");
    }
  // } else if (command == xx) {
  } else {
    Serial.print("Unknown command {");
    Serial.print(command);
    Serial.println("}");
  }

  Wifi.print(EOL);	// terminator
}
#endif

#if 0
void callback(char *topic, byte *payload, unsigned int length) {
  char *pl = (char *)payload;
  char reply[80];

  pl[length] = 0;

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
  if (strcmp(topic, mqtt_topic) == 0) {
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
      if (verbose & VERBOSE_BMP) Serial.printf("BMP 0x%8X\n", bmp);

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
#ifdef SERRE
    } else if (strncmp(pl, mqtt_topic_valve, length) == 0) {
      /*
       * Read valve/pump status
       */
      if (verbose & VERBOSE_VALVE) Serial.println("Topic valve");
      client.publish(mqtt_topic_valve, valve ? "Open" : "Closed");
    } else if (strncmp(pl, mqtt_topic_valve_start, length) == 0) {
      if (verbose & VERBOSE_VALVE) Serial.println("Topic valve start");
      SetState(1);
      ValveOpen();
    } else if (strncmp(pl, mqtt_topic_valve_stop, length) == 0) {
      if (verbose & VERBOSE_VALVE) Serial.println("Topic valve stop");
      SetState(0);
      ValveReset();
#endif
#ifdef KIPPEN
    } else if (strncmp(pl, mqtt_topic_hatch, length) == 0) {
      /*
       * Read valve/pump status
       */
      if (verbose & VERBOSE_VALVE) Serial.println("Topic valve");
      client.publish(mqtt_topic_hatch, hatch ? "Open" : "Closed");
    } else if (strncmp(pl, mqtt_topic_hatch_start, length) == 0) {
      if (verbose & VERBOSE_VALVE) Serial.println("Topic hatch up");
      SetState(1);
      hatch->Up();
    } else if (strncmp(pl, mqtt_topic_hatch_stop, length) == 0) {
      if (verbose & VERBOSE_VALVE) Serial.println("Topic hatch down");
      SetState(0);
      hatch->Down();
#endif
    } else if (strncmp(pl, mqtt_topic_restart, length) == 0) {
      // Always stop water flow before reboot
#ifdef SERRE
      SetState(0);
      ValveReset();
#endif
      // Reboot the ESP without a software update.
      ESP.restart();
    } else if (strncmp(pl, mqtt_topic_current_time, length) == 0) {
      if (verbose & VERBOSE_SYSTEM) Serial.println("Topic current time");

      time_t tsnow = sntp_get_current_timestamp();
      now = localtime(&tsnow);
      strftime(reply, sizeof(reply), "Current time %F %T", now);
      client.publish(mqtt_topic_current_time, reply);

      if (verbose & VERBOSE_SYSTEM) Serial.printf("Reply {%s} {%s}\n", mqtt_topic_current_time, reply);
    } else if (strncmp(pl, mqtt_topic_boot_time, length) == 0) {
      if (verbose & VERBOSE_SYSTEM) Serial.println("Topic boot time");

      now = localtime(&tsboot);
      strftime(reply, sizeof(reply), "Boot %F %T", now);
      client.publish(mqtt_topic_boot_time, reply);

      if (verbose & VERBOSE_SYSTEM) Serial.printf("Reply {%s} {%s}\n", mqtt_topic_boot_time, reply);
    } else if (strncmp(pl, mqtt_topic_reconnects, length) == 0) {
      // Report the number of MQTT reconnects to our broker
      sprintf(reply, "Reconnect count %d", nreconnects);
      client.publish(mqtt_topic_reconnects, reply);
    } else if (strncmp(pl, mqtt_topic_version, length) == 0) {
      // Report the build version
      sprintf(reply, "Build version %s %s", _BuildInfo.date, _BuildInfo.time);
      client.publish(mqtt_topic_version, reply);
    } else if (strncmp(pl, mqtt_topic_info, length) == 0) {
      // Report the build version
      sprintf(reply, "System Info %s", SystemInfo1);
      client.publish(mqtt_topic_info, reply);
      sprintf(reply, "System Info %s", SystemInfo2);
      client.publish(mqtt_topic_info, reply);
    } else if (strncmp(pl, mqtt_topic_schedule, length) == 0) {
#ifdef SERRE
      char *sched = water->getSchedule();
#else
      char *sched = hatch->getSchedule();
#endif
      client.publish(mqtt_topic_schedule, sched);
      Serial.printf("Schedule %s -> %s\n", mqtt_topic_schedule, sched);
      free(sched);
#ifdef SERRE
      // Always stop water flow when schedule is manipulated
      SetState(0);
      ValveReset();
#endif
    }

    // End topic == mqtt_topic_serre

  } else if (strcmp(topic, mqtt_topic_bmp180) == 0) {
    // Ignore, this is a message we've sent out ourselves
#ifdef SERRE
  } else if (strcmp(topic, mqtt_topic_valve) == 0) {
    // Ignore, this is a message we've sent out ourselves
#endif
#ifdef KIPPEN
  } else if (strcmp(topic, mqtt_topic_hatch) == 0) {
    // Ignore, this is a message we've sent out ourselves
#endif
  } else if (strcmp(topic, mqtt_topic_schedule_set) == 0) {
    Serial.printf("Set schedule to %s\n", pl);
#ifdef SERRE
    water->setSchedule(pl);
#else
    hatch->setSchedule(pl);
#endif
  } else if (strcmp(topic, mqtt_topic_verbose) == 0) {
    sscanf(pl, "%d", &verbose);
    Serial.printf("Verbosity set to %d\n", verbose);
  } else {
    Serial.println("Unknown topic");
  }
}

void ProcessCallback(WifiData client) {
  // read the command
#if 1
  String command = client.readString();
  String out = "Server {";
  out += command;
  out += "} ...";
  Wifi.println(out);
  Serial.println(out);
#else
  String command = client.readStringUntil('/');
 
  // is "digital" command?
  if (command == "digital") {
    digitalCommand(client);
  }
 
  // is "analog" command?
  if (command == "analog") {
    analogCommand(client);
  }
 
  // is "mode" command?
  if (command == "mode") {
    modeCommand(client);
  }
#endif
}
 
#if 0
void digitalCommand(WifiData client) {
  int pin, value;
 
  // Read pin number
  pin = client.parseInt();
 
  // If the next character is a '/' it means we have an URL
  // with a value like: "/digital/13/1"
  if (client.read() == '/') {
    value = client.parseInt();
    digitalWrite(pin, value);
  }
  else {
    value = digitalRead(pin);
  }
 
  // Send feedback to client
  // client.println("Status: 200 OK\n");
  // client.print("Pin D");
  // client.print(pin);
  // client.print(" set to ");
  // client.println(value);
  // client.print(EOL);    //char terminator
  client.print("Status: 200 OK\r\n");
  client.print("Pin D");
  client.print(pin);
  client.print(" set to ");
  client.print(value);
  client.print("\r\n");
  client.print(EOL);    //char terminator
 
  Serial.println("Status: 200 OK\n");
  Serial.print("Pin D");
  Serial.print(pin);
  Serial.print(" set to ");
  Serial.println(value);
}
 
void analogCommand(WifiData client) {
  int pin, value;
 
  // Read pin number
  pin = client.parseInt();
 
  // If the next character is a '/' it means we have an URL
  // with a value like: "/analog/5/120"
  if (client.read() == '/') {
    // Read value and execute command
    value = client.parseInt();
    analogWrite(pin, value);
 
    // Send feedback to client
    client.println("Status: 200 OK\n");
    client.print(F("Pin D"));
    client.print(pin);
    client.print(F(" set to analog "));
    client.println(value);
    client.print(EOL);    //char terminator
 
  }
  else {
    // Read analog pin
    value = analogRead(pin);
 
    // Send feedback to client
    client.println("Status: 200 OK\n");
    client.print(F("Pin A"));
    client.print(pin);
    client.print(F(" reads analog "));
    client.println(value);
    client.print(EOL);    //char terminator
 
  }
}
 
void modeCommand(WifiData client) {
  int pin;
 
  // Read pin number
  pin = client.parseInt();
 
  // If the next character is not a '/' we have a malformed URL
  if (client.read() != '/') {
    client.println(F("error"));
    client.print(EOL);    //char terminator
    return;
  }
 
  String mode = client.readStringUntil('\r');
 
  if (mode == "input") {
    pinMode(pin, INPUT);
    // Send feedback to client
    client.println("Status: 200 OK\n");
    client.print(F("Pin D"));
    client.print(pin);
    client.println(F(" configured as INPUT!"));
    client.print(EOL);    //char terminator
    return;
  }
 
  if (mode == "output") {
    pinMode(pin, OUTPUT);
    // Send feedback to client
    client.println("Status: 200 OK\n");
    client.print(F("Pin D"));
    client.print(pin);
    client.println(F(" configured as OUTPUT!"));
    client.print(EOL);    //char terminator
    return;
  }
 
  client.print(F("error: invalid mode "));
  client.println(mode);
  client.print(EOL);    //char terminator
}
#endif
#endif

const char *mqtt_topic_bmp180 =		SystemId "/BMP180";
#if 0
const char *mqtt_topic =		SystemId;
const char *mqtt_topic_bmp180 =		SystemId "/BMP180";
const char *mqtt_topic_hatch =		SystemId "/Hatch";
const char *mqtt_topic_hatch_start =	SystemId "/Hatch/1";
const char *mqtt_topic_hatch_stop =	SystemId "/Hatch/0";
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
#endif
