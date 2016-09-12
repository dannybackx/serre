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

// See http://www.bc-robotics.com/tutorials/controlling-a-solenoid-valve-with-arduino/

#include <ESP8266WiFi.h>
#include <Wire.h>       // Required to compile UPnP
#include "PubSubClient.h"
#include "SFE_BMP180.h"
#include "esptime.h"
#include "ThingSpeak.h"
#include "Ifttt.h"

extern "C" {
#include <ip_addr.h>
#include <espconn.h>
}

// Prepare for OTA software installation
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
static int OTAprev;

#include "mywifi.h"

const char *mqtt_user = MY_MQTT_USER;
const char *mqtt_password = MY_MQTT_PASSWORD;

// One "client" per application / connection
WiFiClient	pubSubEspClient, IfTttEspClient, TSEspClient;
PubSubClient	client(pubSubEspClient);
void ext_mqtt_connect_gethostbyname(const char *server);
Ifttt		*ifttt = 0;

#include "global.h"
#include "Water.h"

int led_pin = 13;		// D11
int speed = 255;		// Value to be written to the controller
int mqtt_initial = 1;

//
// Test results from testwemosd1/testwemosd1 :
// Note this is with an old model Wemos D1 (http://www.wemos.cc/Products/d1.html).
//	value		pin
//	16		D2
//	15		D10 / SS
//	14		D13 / SCK / D5
//	13		D11 / MOSI / D7
//	12		D6 / MISO / D12
//	11		N/A
//	10
//	9
//	8		crashes ?
//	7
//	6
//	5		D15 / SCL / D3
//	4		D14 / SDA / D4
//	3		D0 / RX
//	2		TX1 / D9
//	1		TX breaks serial connection
//	0		D8
//

// All kinds of variables
int		pin14;
int		count = 0;
SFE_BMP180	*bmp = 0;
esptime		*et = 0;

double		newPressure, newTemperature, oldPressure, oldTemperature;
int		percentage;		// How much of a difference before notify
int		valve = 0;		// Closed = 0

time_t		tsnow, tsboot, tsprevious = 0;
int		nreconnects = 0;
struct tm	*now;
char		buffer[64];

Water		*water = NULL;
char		SystemInfo1[80], SystemInfo2[256];

int oldstate = 0, state = 0;

// Forward declarations
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);
void ValveReset(), ValveOpen();
void BMPInitialize();
int BMPQuery();
void RTCInitialize();

// Arduino setup function
void setup() {
  Serial.begin(9600);
  Wire.begin();		// Default SDA / SCL are xx / xx
  
  delay(3000);    // Allow you to plug in the console
  
  Serial.println(startup_text);
  sprintf(SystemInfo1, "Boot version %d, flash chip size %d, SDK version %s",
    ESP.getBootVersion(), ESP.getFlashChipSize(), ESP.getSdkVersion());
  Serial.println(SystemInfo1);

  Serial.printf("Free sketch space %d\n", ESP.getFreeSketchSpace());
  Serial.print("Starting WiFi ");
  WiFi.mode(WIFI_STA);

  // Try to connect to WiFi
  int wcr = mywifi();

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    Serial.printf("WiFi Failed\n");
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  String ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  String gws = gw.toString();
  sprintf(SystemInfo2, "MAC %s SSID {%s}, IP %s, GW %s",
    WiFi.macAddress().c_str(),
    WiFi.SSID().c_str(), ips.c_str(), gws.c_str());
  Serial.println(SystemInfo2);

  // Set allowed TCP sockets to the maximum
  sint8 rc = espconn_tcp_set_max_con(15);
  if (rc < 0) {
    Serial.println("Failed to increase #TCP connections");
  } else {
    uint8 mc = espconn_tcp_get_max_con();
    Serial.printf("TCP connection limit set to %d\n", mc);
  }

  // This one will not fail to create
  et = new esptime();
  et->begin();

  // OTA : allow for software upgrades
  Serial.printf("Starting OTA listener ...\n");
  ArduinoOTA.onStart([]() {
    // Always stop water flow before OTA Software update
    ValveReset();
    Serial.print("OTA Start : ");

    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic_serre, "OTA start");
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Complete");
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic_serre, "OTA complete");
    }
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
    if (OTAprev < curr)
      Serial.print('#');
    OTAprev = curr;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");

    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
      client.publish(mqtt_topic_serre, "OTA Error");
    }
  });
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.begin();

  Serial.printf("Time ");
  tsboot = tsnow = et->now((char *)".");
  now = localtime(&tsnow);
  strftime(buffer, sizeof(buffer), " %F %T", now);
  Serial.println(buffer);

  // IFTTT
#ifdef IFTTT_KEY
  ifttt = new Ifttt(IfTttEspClient);
#endif

  // MQTT
  if (external_network) {
    Serial.println("Starting MQTT (external network)");
    ext_mqtt_connect_gethostbyname(MQTT_EXT_SERVER);
  } else {
    Serial.println("Starting MQTT");
    client.setServer(mqtt_server, mqtt_port);
  }
  client.setCallback(callback);

  // BMP180 temperature and air pressure sensor
  Serial.print("Initializing BMP180 ... ");
  BMPInitialize();
  Serial.println(bmp ? "ok" : "failed");

  // Alert our owner via IFTTT if the sensor didn't initialize well
#ifdef IFTTT_KEY
  if (bmp == NULL) {
    ifttt->sendEvent((char *)IFTTT_KEY, (char *)IFTTT_EVENT,
      (char *)"BMP180 Initialisation failure", (char *)"ab", (char *)"cde");
  } else {
    ifttt->sendEvent((char *)IFTTT_KEY, (char *)IFTTT_EVENT,
      (char *)"Boot report", SystemInfo1, SystemInfo2);
  }
#endif

  pinMode(valve_pin, OUTPUT);
  pinMode(led_pin, OUTPUT);

  water = new Water(watering_schedule_string);

  // Thingspeak
  ThingSpeak.begin(TSEspClient);

  Serial.println("Ready");
}

void loop() {
  count++;
  ArduinoOTA.handle();
  et->loop();

  analogWrite(led_pin, count % 256);

  // MQTT
  if (!client.connected()) {
    reconnect();
  }
  if (!client.loop())
    reconnect();

  // Save old state
  oldstate = state;

  // Get the current time
  tsnow = et->now(NULL);
  now = localtime(&tsnow);
  state = water->loop(now->tm_hour, now->tm_min);

  // Serial.printf("TS %08x Water(%d,%d) -> %d\n", tsnow, now->tm_hour, now->tm_min, state);

  if (state != oldstate) {
    // Report change
    Serial.printf("Changed from %d to %d at %2d:%2d\n", oldstate, state, now->tm_hour, now->tm_min);
#ifdef THINGSPEAK_CHANNEL
    Serial.printf("Feeding ThingSpeak field 3, %d %d -> %d\n", now->tm_hour, now->tm_min, state);
    ThingSpeak.setField(3, state);
    ThingSpeak.writeFields(THINGSPEAK_CHANNEL, THINGSPEAK_WRITE);
#else
#warning no ThingSpeak
#endif
  }

  delay(100);

  if (state == 0) {
    ValveReset();
  } else {
    ValveOpen();
  }

  // Periodic reporting
  if (tsprevious == 0 || tsnow - tsprevious > report_frequency_seconds) {
    tsprevious = tsnow;

    // Send stuff
    if (bmp) {
      int bmperror;
      // Query the sensor
      bmperror = BMPQuery();
#if 0
      Serial.printf("BMPQuery -> %d\n", bmperror);
      {
      int a, b, c;
      a = newTemperature;
      b = (newTemperature - a) * 100;
      c = newPressure;
      Serial.printf("BMPQuery temp %d.%02d pres %d\n", a, b, c);
      }
#endif
      if ((bmperror = BMPQuery()) != 0) {
        // Error
	char e[20];
	sprintf(e, "%d", bmperror);
#ifdef IFTTT_KEY
	ifttt->sendEvent((char *)IFTTT_KEY, (char *)IFTTT_EVENT, (char *)"BMP failure", e);
#endif
      } else {
	// BMP180 results ok

#ifdef THINGSPEAK_CHANNEL
      // Send the result
      ThingSpeak.setField(1, (float)newTemperature);
      ThingSpeak.setField(2, (float)newPressure);
      ThingSpeak.writeFields(THINGSPEAK_CHANNEL, THINGSPEAK_WRITE);
      Serial.printf("Feeding ThingSpeak channels 1+2\n");
#endif
      }
    }
  }
}

void ValveOpen() {
  valve = 1;
  // digitalWrite(valve_pin, 1);
  analogWrite(valve_pin, speed);
}

void ValveReset() {
  valve = 0;
  // digitalWrite(valve_pin, 0);
  analogWrite(valve_pin, 0);
}

void reconnect(void) {
  nreconnects++;

  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_HOSTNAME, mqtt_user, mqtt_password)) {
      Serial.println("connected");

      // Get the current time
      tsnow = et->now(NULL);
      now = localtime(&tsnow);

      // Once connected, publish an announcement...
      if (mqtt_initial) {
	strftime(buffer, sizeof(buffer), "boot %F %T", now);
        client.publish(mqtt_topic_serre, buffer);

	mqtt_initial = 0;
      } else {
	strftime(buffer, sizeof(buffer), "reconnect %F %T", now);
        client.publish(mqtt_topic_serre, buffer);
      }

      // ... and (re)subscribe
      client.subscribe(mqtt_topic_serre);
      client.subscribe(mqtt_topic_valve);
      client.subscribe(mqtt_topic_bmp180);
      client.subscribe(mqtt_topic_verbose);
      client.subscribe(mqtt_topic_schedule);
      client.subscribe(mqtt_topic_schedule_set);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

extern "C" {
void Debug(char *s) {
  Serial.print(s);
}

void DebugInt(int i) {
  Serial.print(i);
}
}

void BMPInitialize() {
  bmp = new SFE_BMP180();
  char ok = bmp->begin();	// 0 return value is a problem
  if (ok == 0) {
    delete bmp;
    bmp = 0;
  }
}

/*
 * Returns 0 on success, negative values on error
 * -1 not initialized
 * -2xx nan
 * -3 communication error
 */
int BMPQuery() {
  bool nan = false;

  if (bmp == 0)
    return -1;

  // This is a multi-part query to the I2C device, see the SFE_BMP180 source files.
  char d1 = bmp->startTemperature();
  delay(d1);
  char d2 = bmp->getTemperature(newTemperature);
  if (d2 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (temperature)\n");
#endif
    return -201;
  }

  char d3 = bmp->startPressure(0);
  delay(d3);
  char d4 = bmp->getPressure(newPressure, newTemperature);
  if (d4 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (pressure)\n");
#endif
    return -202;
  }

  if (isnan(newTemperature) || isinf(newTemperature)) {
    newTemperature = oldTemperature;
    nan = true;
  }
  if (isnan(newPressure) || isinf(newPressure)) {
    newPressure = oldPressure;
    nan = true;
  }
  if (nan) {
#ifdef DEBUG
    DEBUG.println("BMP180 nan");
#endif
    return -203;
  }
  return 0;
}

void SetState(int s) {
  state = s;
  water->set(s);
}

static void _dns_found_cb(const char *name, ip_addr_t *ipaddr, void *arg)
{
  // Serial.printf("MQTT connect to %d.%d.%d.%d\n",
  //   ip4_addr1(ipaddr), ip4_addr2(ipaddr), ip4_addr3(ipaddr), ip4_addr4(ipaddr));
  client.setServer((uint8_t *)ipaddr, MQTT_EXT_PORT);
}

void ext_mqtt_connect_gethostbyname(const char *server)
{
   ip_addr_t ip_addr;
   switch (espconn_gethostbyname(NULL, server, &ip_addr, _dns_found_cb))
   {
   case ESPCONN_INPROGRESS:
      Serial.println("Lookup in progress");
      #ifdef PLATFORM_DEBUG
      os_printf("SMTP DNS lookup for %s\r\n", server);
      #endif
      break;

   case ESPCONN_OK:
      Serial.println("Lookup ok");
      _dns_found_cb(server, &ip_addr, NULL);
      break;
      
   case ESPCONN_ARG:
      Serial.println("Lookup arg error");
      #ifdef PLATFORM_DEBUG
      os_printf("SMTP DNS argument error %s\n", server);
      #endif
      break;
      
   default:   
      Serial.println("Lookup default");
      #ifdef PLATFORM_DEBUG
      os_printf("SMTP DNS lookup error\r\n");
      #endif
      break;
   }
}
