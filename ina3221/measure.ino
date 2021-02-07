#undef	DEBUG
/*
 * Measurement station, with MQTT and a web server
 *
 * Copyright (c) 2016, 2017, 2019, 2020, 2021 Danny Backx
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
#include <PubSubClient.h>
// #include <TimeLib.h>
// #include <stdarg.h>

#include <coredecls.h>
#include <TZ.h>

extern "C" {
#include <sntp.h>
}

// Prepare for OTA software installation
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
static int OTAprev;

#include "secrets.h"
#include "personal.h"
#include "buildinfo.h"

struct mywifi {
  const char *ssid, *pass;
} mywifi[] = {
#ifdef MY_SSID_1
  { MY_SSID_1, MY_WIFI_PASSWORD_1 },
#endif
#ifdef MY_SSID_2
  { MY_SSID_2, MY_WIFI_PASSWORD_2 },
#endif
#ifdef MY_SSID_3
  { MY_SSID_3, MY_WIFI_PASSWORD_3 },
#endif
#ifdef MY_SSID_4
  { MY_SSID_4, MY_WIFI_PASSWORD_4 },
#endif
#ifdef MY_SSID_5
  { MY_SSID_5, MY_WIFI_PASSWORD_5 },
#endif
  { NULL, NULL}
};

// MQTT
WiFiClient	espClient;
PubSubClient	client(espClient);

const char* mqtt_server = MQTT_HOST;
const int mqtt_port = MQTT_PORT;

const char *mqtt_topic = SWITCH_TOPIC;
const char *reply_topic = SWITCH_TOPIC "/reply";
const char *relay_topic = SWITCH_TOPIC "/relay";

int mqtt_initial = 1;
int _isdst = 0;

// Forward
void RelayDebug(const char *format, ...);
void Debug(const char *format, ...);
void callback(char * topic, byte *payload, unsigned int length);
void reconnect(void);
void SetDefaultSchedule(time_t);
void time_is_set(bool from_sntp);

// All kinds of variables
int		verbose = 0;
String		ips, gws;
int		state = 0;
int		manual = 0;
WiFiServer	server(80);

#define	VERBOSE_OTA	0x01
#define	VERBOSE_VALVE	0x02
#define	VERBOSE_BMP	0x04
#define	VERBOSE_4	0x08

// Arduino setup function
void setup() {
  Serial.begin(115200);
  Serial.printf("Starting %s", SWITCH_TOPIC);

  // Try to connect to WiFi
  WiFi.mode(WIFI_STA);

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      Serial.printf("\nTrying (%d %d) %s %s .. ", ix, wifi_tries, mywifi[ix].ssid, mywifi[ix].pass);
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
    }
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    delay(2000);
    ESP.restart();
  }

  IPAddress ip = WiFi.localIP();
  ips = ip.toString();
  IPAddress gw = WiFi.gatewayIP();
  gws = gw.toString();

  Serial.printf("connected, ip %s, gw %s\n", ips.c_str(), gws.c_str());

  configTime(TZ_Europe_Brussels, "ntp.telenet.be");
  time_t local, utc;
  localtime(&local);
  gmtime(&utc);

  settimeofday_cb(time_is_set);
  server.begin();

  ArduinoOTA.onStart([]() {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
#ifdef DEBUG
      Serial.printf("OTA start\n");
#endif
      client.publish(reply_topic, "OTA start");
    }
  });

  ArduinoOTA.onEnd([]() {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
#ifdef DEBUG
      Serial.printf("OTA complete\n");
#endif
      client.publish(reply_topic, "OTA complete");
    }
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
    OTAprev = curr;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if (verbose & VERBOSE_OTA) {
      if (!client.connected())
        reconnect();
#ifdef DEBUG
      Serial.printf("OTA error\n");
#endif
      client.publish(reply_topic, "OTA Error");
    }
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);

  ArduinoOTA.begin();

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Note don't use MQTT here yet, we're probably not connected yet

}

int	counter = 0;
int	oldminute, newminute, oldhour, newhour;
time_t	the_time;

void loop() {
  ArduinoOTA.handle();

  // MQTT
  if (!client.connected()) {
    reconnect();
  }
  if (!client.loop())
    reconnect();

  delay(100);

  counter++;

  WiFiClient client = server.available();
  if (client) {
    Serial.print("Client connected\n");
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
      }

      client.println("HTTP/1.1 200 OK");
      client.println("Content-type: text/html");
      client.println("Connection: close");
      client.println();

      client.println("<!DOCTYPE html><html>");
      client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
      client.println("<link rel=\"icon\" href=\"data:,\">");
      // CSS to style the on/off buttons
      // Feel free to change the background-color and font-size attributes to fit your preferences
      client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
      client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
      client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
      client.println(".button2 {background-color: #77878A;}</style></head>");

      // Web Page Heading
      client.println("<title>ESP8266 Web Server</title>");
      client.println("<body><h1>ESP8266 Web Server</h1>");

      // The HTTP responds ends with another blank line
      client.println("</body></html>");
      client.println("");
      break;
    }
    client.stop();
  }

  // Only do this once per second or so (given the delay 100)
  if (counter % 10 == 0) {
    counter = 0;

    oldminute = newminute;
    oldhour = newhour;

    the_time = time(0);
    struct tm *tmp = localtime(&the_time);

    newhour = tmp->tm_hour;
    newminute = tmp->tm_min;

    if (newminute == oldminute && newhour == oldhour) {
      return;
    }

    int the_day = tmp->tm_mday;
    int the_month = tmp->tm_mon + 1;
    int the_year = tmp->tm_year + 1900;
    int the_dow = tmp->tm_wday;

    if (newminute == 1 && newhour == 1) {
      // SetDefaultSchedule(the_time);
    }
  }
}

/*
 * Respond to MQTT queries.
 * Only two types are subscribed to :
 *   topic == "/switch"			payload contains subcommand
 *   topic == "/switch/program"		payload contains new schedule
 */
void callback(char *topic, byte *payload, unsigned int length) {
  char reply[80];

  char pl[80];
  strncpy(pl, (const char *)payload, length);
  pl[length] = 0;

  // RelayDebug("Callback topic {%s} payload {%s}", topic, pl);

  if (strcmp(topic, SWITCH_TOPIC) == 0) {
           if (strcmp(pl, "on") == 0) {	// Turn the relay on
      // PinOn();
    } else if (strcmp(pl, "off") == 0) {	// Turn the relay off
      // PinOff();
    } else if (strcmp(pl, "state") == 0) {	// Query on/off state
      // RelayDebug("Switch %s", GetState() ? "on" : "off");
    } else if (strcmp(pl, "query") == 0) {	// Query schedule
#ifdef DEBUG
      Serial.printf("%s %s %s\n", SWITCH_TOPIC, "/schedule", GetSchedule());
#endif
      // RelayDebug("schedule %s", GetSchedule());
    } else if (strcmp(pl, "network") == 0) {	// Query network parameters
      Debug("SSID {%s}, IP %s, GW %s", WiFi.SSID().c_str(), ips.c_str(), gws.c_str());
    } else if (strcmp(pl, "time") == 0) {	// Query the device's current time
      time_t t = time(0);
      tm *tmp = localtime(&t);
      RelayDebug("%04d.%02d.%02d %02d:%02d:%02d",
	tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    } else if (strcmp(pl, "reboot") == 0) {
      ESP.restart();
    } else if (strcmp(pl, "reinit") == 0) {
      // mySntpInit();
    }
    // else silently ignore
  } else if (strcmp(topic, SWITCH_TOPIC "/program") == 0) {
    // Set the schedule according to the string provided
    // SetSchedule((char *)pl);
    client.publish(SWITCH_TOPIC "/schedule", "Ok");
  }
  // Else silently ignore
}

void reconnect(void) {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect(MQTT_CLIENT)) {
      // Get the time
      time_t t = time(0);
      struct tm *tmp = localtime(&t);

      // Once connected, publish an announcement...
      if (t > 5000) {
        if (mqtt_initial) {
	  Debug("boot %04d.%02d.%02d %02d:%02d:%02d",
	    tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	  mqtt_initial = 0;
        } else {
          Debug("reconnect %04d.%02d.%02d %02d:%02d:%02d",
	    tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
        }
      }

      // ... and (re)subscribe
      client.subscribe(SWITCH_TOPIC);
      client.subscribe(SWITCH_TOPIC "/program");
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Send a line of debug info to MQTT
void RelayDebug(const char *format, ...)
{
  char buffer[128];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, 128, format, ap);
  va_end(ap);
  client.publish(relay_topic, buffer);
#ifdef DEBUG
  Serial.printf("%s\n", buffer);
#endif
}

// Send a line of debug info to MQTT
void Debug(const char *format, ...)
{
  char buffer[128];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, 128, format, ap);
  va_end(ap);
  client.publish(reply_topic, buffer);
#ifdef DEBUG
  Serial.printf("%s\n", buffer);
#endif
}

void time_is_set(bool from_sntp) {
  time_t now = time(0);
  struct tm *tmp = localtime(&now);

  Serial.printf("settimeofday(%s), time %04d.%02d.%02d %02d:%02d:%02d\n",
    from_sntp ? "SNTP" : "USER",
    tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

  RelayDebug("Time is %04d.%02d.%02d %02d:%02d:%02d",
    tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}
