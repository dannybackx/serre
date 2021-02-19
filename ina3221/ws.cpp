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
// #include "aht10.h"
#include <Control.h>

ESP8266WebServer	*ws = 0;

// Version with chunked response
static void handleRoot() {
  if (! ws->chunkedResponseModeStart(200, "text/html")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    return;
  }

  Serial.printf("Replying to HTTP client ... ");
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
#if 0
  for (int i=0; i<100; i++)
    if (aht_reg[i].ts != 0) {
      char line[80];
      tm *tmp = localtime(&aht_reg[i].ts);
      sprintf(line, "<tr><td>%04d.%02d.%02d %02d:%02d:%02d</td><td>%3.1f</td><td>%2.0f</td></tr>\n",
	tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
	aht_reg[i].temp, aht_reg[i].hum);
      ws->sendContent(line);
    }
#else
  // Serial.printf("Allocation %d", control->getAllocation());
  for (int i=0; i<control->getAllocation(); i++) {
    time_t ts = control->getTimestamp(i);
    // Serial.printf("TS %d %d\n", i, ts);
    if (ts == 0)
      continue;
    tm *tmp = localtime(&ts);
    float t, h;
    t = control->getDataFloat(i, 0, 0);
    h = control->getDataFloat(i, 0, 1);
    char line[80];
    sprintf(line, "<tr><td>%04d.%02d.%02d %02d:%02d:%02d</td><td>%3.1f</td><td>%2.0f</td></tr>\n",
      tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
      t, h);
    ws->sendContent(line);
  }
#endif
  ws->sendContent("</tr></table>\n");

  // The HTTP responds ends with another blank line
  ws->sendContent("</body></html>");
  ws->chunkedResponseFinalize();
  Serial.printf("done\n");
}

void handleConfig() {
}

void handleJson() {
}

void handleWildcard() {
  Serial.printf("WS: %s\n", ws->pathArg(0));
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
