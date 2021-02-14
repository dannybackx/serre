#define	DO_OTA
#define	DO_AHT
#define	DO_WEBSERVER

/*
 * Measurement station, with web server
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

#include <coredecls.h>
#include <TZ.h>

#ifdef DO_OTA
// Prepare for OTA software installation
#include <ArduinoOTA.h>
static int OTAprev;
void doOTA();
#endif

#include "secrets.h"
#include "measure.h"
#include "buildinfo.h"

#ifdef	DO_AHT
#include "aht10.h"
#endif

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

WiFiClient	espClient;
const char *app_name = APP_NAME;

// Forward
void time_is_set(bool from_sntp);

// All kinds of variables
int		verbose = 0;
String		ips, gws;
int		state = 0;
int		manual = 0;

#ifdef	DO_WEBSERVER
#include <ESP8266WebServer.h>
ESP8266WebServer	*ws = 0;
void handleRoot();
void handleNotFound();
#endif

#define	VERBOSE_OTA	0x01
#define	VERBOSE_VALVE	0x02
#define	VERBOSE_BMP	0x04
#define	VERBOSE_4	0x08

// Arduino setup function
void setup() {
  Serial.begin(115200);
  Serial.printf("Starting %s (built %s %s on v%s)", app_name, _BuildInfo.date, _BuildInfo.time, _BuildInfo.env_version);

  // Try to connect to WiFi
  WiFi.mode(WIFI_STA);

  int wcr = WL_IDLE_STATUS;
  for (int ix = 0; wcr != WL_CONNECTED && mywifi[ix].ssid != NULL; ix++) {
    int wifi_tries = 3;
    while (wifi_tries-- >= 0) {
      Serial.printf("\nTrying (%d %d) %s .. ", ix, wifi_tries, mywifi[ix].ssid);
      WiFi.begin(mywifi[ix].ssid, mywifi[ix].pass);
      wcr = WiFi.waitForConnectResult();
      if (wcr == WL_CONNECTED)
        break;
    }
  }

  // Reboot if we didn't manage to connect to WiFi
  if (wcr != WL_CONNECTED) {
    Serial.printf("not connected, restarting .. ");
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

#ifdef	DO_WEBSERVER
  ws = new ESP8266WebServer(80);
  
  ws->on("/", handleRoot);
  ws->onNotFound(handleNotFound);
  ws->on("/gif", []() {
    static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = millis() % 256;
    gif_colored[17] = millis() % 256;
    gif_colored[18] = millis() % 256;
    ws->send(200, "image/gif", gif_colored, sizeof(gif_colored));
  });


  ws->begin();
  Serial.printf("Web server started, port %d\n", 80);
#endif

#ifdef	DO_OTA
  doOTA();
#endif

#ifdef	DO_AHT
  aht10_begin();
#endif
}

int	old_minute, old_hour;
time_t	the_time;
int	the_day, the_month, the_year, the_dow, the_hour, the_minute;

void loop() {
  // Get the time
  old_minute = the_minute;
  old_hour = the_hour;

  the_time = time(0);
  struct tm *tmp = localtime(&the_time);

  the_hour = tmp->tm_hour;
  the_minute = tmp->tm_min;

  the_day = tmp->tm_mday;
  the_month = tmp->tm_mon + 1;
  the_year = tmp->tm_year + 1900;
  the_dow = tmp->tm_wday;

#ifdef DO_OTA
  ArduinoOTA.handle();
#endif

  delay(1);

#ifdef DO_AHT
  aht10_loop(the_time);
#endif

#ifdef	DO_WEBSERVER
  if (ws)
    ws->handleClient();
#endif
}

void time_is_set(bool from_sntp) {
  time_t now = time(0);
  struct tm *tmp = localtime(&now);

  Serial.printf("settimeofday(%s), time %04d.%02d.%02d %02d:%02d:%02d\n",
    from_sntp ? "SNTP" : "USER",
    tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

void doOTA() {
  Serial.printf("Configuring OTA\n");

#ifdef DO_OTA
  ArduinoOTA.onStart([]() {
    if (verbose & VERBOSE_OTA) {
#ifdef DEBUG
      Serial.printf("OTA start\n");
#endif
    }
  });

  ArduinoOTA.onEnd([]() {
    if (verbose & VERBOSE_OTA) {
#ifdef DEBUG
      Serial.printf("OTA complete\n");
#endif
    }
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int curr;
    curr = (progress / (total / 50));
    OTAprev = curr;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if (verbose & VERBOSE_OTA) {
#ifdef DEBUG
      Serial.printf("OTA error\n");
#endif
    }
  });

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_ID);
  WiFi.hostname(OTA_ID);

  ArduinoOTA.begin();
#endif 	/* OTA */
}

char *timestamp(time_t t) {
  static char ret[80];
  tm *tmp = localtime(&t);
  sprintf(ret, "%04d.%02d.%02d %02d:%02d:%02d",
	tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
  return &ret[0];
}

void handleRootInefficient() {
  Serial.printf("Replying to HTTP client ...");
  String r = 
	"<!DOCTYPE html><html>"
	"<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
	"<link rel=\"icon\" href=\"data:,\">"
	// CSS to style the on/off buttons
	// Feel free to change the background-color and font-size attributes to fit your preferences
	"<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
	".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;"
	"text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
	".button2 {background-color: #77878A;}</style></head>"

	// Web Page Heading
	"<title>ESP8266 Web Server</title>"
	"<body><h1>ESP8266 Web Server</h1>\n";

  r += "<table><tr><td>time</td><td>temperature</td><td>humidity</td></tr>\n";
  for (int i=0; i<100; i++)
    if (aht_reg[i].ts != 0) {
      char line[80];
      tm *tmp = localtime(&aht_reg[i].ts);
      sprintf(line, "<tr><td>%04d.%02d.%02d %02d:%02d:%02d</td><td>%3.1f</td><td>%2.0f</td></tr>\n",
	tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
	aht_reg[i].temp, aht_reg[i].hum);
      r += line;
    }
  r += "</tr></table>\n";

  // The HTTP responds ends with another blank line
  r += "</body></html>";
  ws->send(200, "text/html", r);
  Serial.printf("done\n");
}

// Version with chunked response
void handleRoot() {
  if (! ws->chunkedResponseModeStart(200, "text/html")) {
    handleRootInefficient();
    return;
  }

  Serial.printf("Replying to HTTP client (chunked) ...");
  ws->sendContent(
	"<!DOCTYPE html><html>"
	"<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
	"<link rel=\"icon\" href=\"data:,\">"
	// CSS to style the on/off buttons
	// Feel free to change the background-color and font-size attributes to fit your preferences
	"<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
	".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;"
	"text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
	".button2 {background-color: #77878A;}</style></head>"

	// Web Page Heading
	"<title>ESP8266 Web Server</title>"
	"<body><h1>ESP8266 Web Server</h1>\n");

  ws->sendContent("<table><tr><td>time</td><td>temperature</td><td>humidity</td></tr>\n");
  for (int i=0; i<100; i++)
    if (aht_reg[i].ts != 0) {
      char line[80];
      tm *tmp = localtime(&aht_reg[i].ts);
      sprintf(line, "<tr><td>%04d.%02d.%02d %02d:%02d:%02d</td><td>%3.1f</td><td>%2.0f</td></tr>\n",
	tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
	aht_reg[i].temp, aht_reg[i].hum);
      ws->sendContent(line);
    }
  ws->sendContent("</tr></table>\n");

  // The HTTP responds ends with another blank line
  ws->sendContent("</body></html>");
  ws->chunkedResponseFinalize();
  Serial.printf("done\n");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += ws->uri();
  message += "\nMethod: ";
  message += (ws->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += ws->args();
  message += "\n";
  for (uint8_t i = 0; i < ws->args(); i++) {
    message += " " + ws->argName(i) + ": " + ws->arg(i) + "\n";
  }
  ws->send(404, "text/plain", message);
}
