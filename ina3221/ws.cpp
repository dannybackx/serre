/*
 * Web server for measurement station
 *
 * Copyright (c) 2021 Danny Backx
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
#include <ESP8266WebServer.h>

#include <coredecls.h>
#include <TZ.h>

#include "measure.h"
#include "ws.h"
#include <Control.h>

ESP8266WebServer	*ws = 0;

static void QuerySensor(int sid, bool html) {
    const char *sn = control->getSensorName(sid);

    if (sn == 0)
      return;

    char line[80];
    if (html)
      sprintf(line, "<H1>Sensor %s</H1>\n<table border=1><tr>\n", sn);
    else
      sprintf(line, "{\"sensor\": \"%s\", ", sn);
    ws->sendContent(line);

    if (html)
      ws->sendContent("<td>timestamp</td>\n");
    for (int i=0; i<MAX_FIELDS; i++) {
      const char *fn = control->getFieldName(sid, i);
      if (fn) {
	if (html) {
          sprintf(line, "<td>%s</td>", fn);
          ws->sendContent(line);
	}
      }
    }
    if (html)
      ws->sendContent("</tr>\n");
    else
      ws->sendContent("\"values\": [\n");

    for (int i=0; i<control->getAllocation(); i++) {
      time_t ts = control->getTimestamp(i);
      if (ts == 0)
        continue;

      tm *tmp = localtime(&ts);

      // Is this data chunk about the sensor under investigation ?
      int sn = control->getDataSensor(i);
      if (sid != sn)
        continue;

      float t, h;
      t = control->getDataFloat(i, 0);
      const char *fn[4];
        fn[0] = "timestamp";
      	fn[1] = control->getFieldName(sid, 0);
	fn[2] = control->getFieldName(sid, 1);
      h = control->getDataFloat(i, 1);

      char line[80];
      if (html)
        sprintf(line, "<tr><td>%04d.%02d.%02d %02d:%02d:%02d</td><td>%3.1f</td><td>%2.0f</td></tr>\n",
          tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
          t, h);
      else
        sprintf(line, "{\"%s\": \"%04d.%02d.%02d %02d:%02d:%02d\", \"%s\": \"%3.1f\", \"%s\": \"%2.0f\"},\n",
	  fn[0], tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
          fn[1], t,
	  fn[2], h);
      ws->sendContent(line);
    }
    if (html)
      ws->sendContent("</tr></table>\n");
    else
      ws->sendContent("]}\n");
}

static void QuerySensors(bool html) {
  if (html) {
  // Version with chunked response
    if (! ws->chunkedResponseModeStart(200, "text/html")) {
      ws->send(500, "Want HTTP/1.1 for chunked responses");
      return;
    }
  
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
  } else {
    if (! ws->chunkedResponseModeStart(200, "text/json")) {
      ws->send(500, "Want HTTP/1.1 for chunked responses");
      return;
    }
    ws->sendContent("{");
  }

  for (int sid = 0; sid < MAX_SENSORS; sid++) {
    QuerySensor(sid, html);
    if (! html) {
      const char *sn = control->getSensorName(sid);
      if (control->getSensorName(sid) && control->getSensorName(sid+1))
         ws->sendContent(",\n");
    }
    // if (sid < MAX_SENSORS && !html)
    //   ws->sendContent(",\n");
  }

  if (html) {
    ws->sendContent("</body></html>");
  } else {
    ws->sendContent("}\n");
  }
  ws->chunkedResponseFinalize();
}

static void handleRoot() {
  QuerySensors(true);
}

static void handleConfig() {
}

static void handleJson() {
  QuerySensors(false);
}

static void handleWildcard() {
  Serial.printf("WS: %s\n", ws->pathArg(0));
}

static void handleNotFound() {
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

void ws_begin() {
  ws = new ESP8266WebServer(80);
  
  ws->on("/", handleRoot);
  ws->onNotFound(handleNotFound);
  ws->on("/json", handleJson);
  ws->on("/config", handleConfig);
#if 0
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
#endif

  ws->begin();
  Serial.printf("Web server started, port %d\n", 80);
}

void ws_loop() {
  if (ws)
    ws->handleClient();
}
