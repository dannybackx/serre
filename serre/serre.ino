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

// Prepare for OTA software installation
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
static int OTAprev;

#include "mywifi.h"
const char *ssid     = MY_SSID;
const char *password = MY_WIFI_PASSWORD;
const char *mqtt_user = MY_MQTT_USER;
const char *mqtt_password = MY_MQTT_PASSWORD;

// MQTT
WiFiClient	espClient;
PubSubClient	client(espClient);

#include "global.h"
#include "Water.h"

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
// char		temperature[BMP180_STATE_LENGTH], pressure[BMP180_STATE_LENGTH];
double		newPressure, newTemperature, oldPressure, oldTemperature;
int		percentage;		// How much of a difference before notify
int		valve = 0;		// Closed = 0

time_t		tsnow, tsboot;
int		nreconnects = 0;
struct tm	*now;
char		buffer[64];

Water		*water = NULL;

int oldstate = 0, state = 0;

// Forward declarations
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);
void ValveReset(), ValveOpen();
void BMPInitialize(), BMPQuery();

// Arduino setup function
void setup() {
  Serial.begin(9600);
  Wire.begin();		// Default SDA / SCL are xx / xx
  
  delay(3000);    // Allow you to plug in the console
  
  Serial.println(startup_text);
  Serial.printf("Boot version %d, flash chip size %d, SDK version %s\n",
    ESP.getBootVersion(), ESP.getFlashChipSize(), ESP.getSdkVersion());

  Serial.printf("Free sketch space %d\n", ESP.getFreeSketchSpace());
  Serial.print("Starting WiFi ");
  WiFi.mode(WIFI_STA);

  // Try to connect to WiFi
  int wifi_tries = 3;
  int wcr;
  while (wifi_tries-- >= 0) {
    WiFi.begin(ssid, password);
    Serial.print(". ");
    wcr = WiFi.waitForConnectResult();
    if (wcr == WL_CONNECTED)
      break;
    Serial.printf(" %d ", wifi_tries + 1);
  }

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
  Serial.print("MAC "); Serial.print(WiFi.macAddress());
  Serial.printf(", SSID {%s}, IP %s, GW %s\n", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());

  // Set up real time clock
  Serial.println("Initialize SNTP ...");
  sntp_init();
  sntp_setservername(0, "ntp.scarlet.be");
  sntp_setservername(1, "ntp.belnet.be");

  // FIXME
  // This is a bad idea : fixed time zone, no DST processing, .. but it works for now.
  // (void)sntp_set_timezone(+2);
  tzset();

  // OTA : allow for software upgrades
  Serial.printf("Starting OTA listener ...\n");
  ArduinoOTA.onStart([]() {
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
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("OTA-Serre");
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.begin();

  // SNTP : wait for a correct time, and report it
  Serial.printf("Time ");
  tsboot = tsnow = sntp_get_current_timestamp();
  while (tsnow < 0x1000) {
    Serial.printf(".");
    delay(1000);
    tsboot = tsnow = sntp_get_current_timestamp();
  }
  now = localtime(&tsnow);
  strftime(buffer, sizeof(buffer), " %F %T", now);
  Serial.println(buffer);

  // MQTT
  Serial.print("Starting MQTT ");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  Serial.println("done");

  // BMP180 temperature and air pressure sensor
  Serial.print("Initializing BMP180 ... ");
  BMPInitialize();
  Serial.println(bmp ? "ok" : "failed");

  pinMode(valve_pin, OUTPUT);

  water = new Water(watering_schedule_string);

  Serial.println("Ready");
}

void loop() {
  ArduinoOTA.handle();

  // MQTT
  if (!client.connected()) {
    reconnect();
  }
  if (!client.loop())
    reconnect();

  // Save old state
  oldstate = state;

  // Get the current time
  tsnow = sntp_get_current_timestamp();
  now = localtime(&tsnow);
  state = water->loop(now->tm_hour, now->tm_min);

  if (state != oldstate) {
    // Report change
    Serial.printf("Changed from %d to %d at %2d:%2d\n", oldstate, state, now->tm_hour, now->tm_min);
  }

  delay(100);

#if 1
  if (state == 0) {
    ValveReset();
  } else {
    ValveOpen();
  }
#else
  count++;
  if (count > 10) {
    ValveReset();

    if (verbose & VERBOSE_VALVE) {
      if (count == 11) client.publish(mqtt_topic_valve, "0");
    }
  } else {
    ValveOpen();
    if (verbose & VERBOSE_VALVE) {
      if (count == 1) client.publish(mqtt_topic_valve, "1");
    }
  }
  if (count == 20)
    count = 0;
#endif
}

void ValveOpen() {
  valve = 1;
  digitalWrite(valve_pin, 1);
}

void ValveReset() {
  valve = 0;
  digitalWrite(valve_pin, 0);
}

void reconnect(void) {
  nreconnects++;

  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");

      // Get the current time
      tsnow = sntp_get_current_timestamp();
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
#ifdef ENABLE_I2C
  bmp = new SFE_BMP180();
  char ok = bmp->begin();	// 0 return value is a problem
  if (ok == 0) {
    delete bmp;
    bmp = 0;
  }
#endif
}

void BMPQuery() {
  if (bmp == 0)
    return;

  // This is a multi-part query to the I2C device, see the SFE_BMP180 source files.
  char d1 = bmp->startTemperature();
  delay(d1);
  char d2 = bmp->getTemperature(newTemperature);
  if (d2 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (temperature)\n");
#endif
    return;
  }

  char d3 = bmp->startPressure(0);
  delay(d3);
  char d4 = bmp->getPressure(newPressure, newTemperature);
  if (d4 == 0) { // Error communicating with device
#ifdef DEBUG
    DEBUG.printf("BMP180 : communication error (pressure)\n");
#endif
    return;
  }

  if (isnan(newTemperature) || isinf(newTemperature)) {
    newTemperature = oldTemperature;
    // nan = true;
  }
  if (isnan(newPressure) || isinf(newPressure)) {
    newPressure = oldPressure;
    // nan = true;
  }
  if (nan) {
#ifdef DEBUG
    DEBUG.println("BMP180 nan");
#endif
    return;
  }
}
