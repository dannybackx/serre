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

#include <uri/UriRegex.h>

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
  for (int sid = 0; sid < MAX_SENSORS; sid++) {
    QuerySensor(sid, html);
    if (! html) {
      const char *sn = control->getSensorName(sid);
      if (control->getSensorName(sid) && control->getSensorName(sid+1))
         ws->sendContent(",\n");
    }
  }

  if (html) {
    ws->sendContent("</body></html>");
  } else {
    ws->sendContent("}\n");
  }
  ws->chunkedResponseFinalize();
}

static void handleHtmlQuery() {
  const char *uri = ws->uri().c_str();

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

  // Query everything
  if (uri == 0 || uri[1] == 0) {
    QuerySensors(true);
    return;
  }

  // Query for an individual sensor
  for (int sid=0; sid<MAX_SENSORS; sid++) {
    const char *sn = control->getSensorName(sid);
    if (sn == 0)
      continue;
    if (strcmp(sn, uri+1) == 0) {	// Prefixed by "/"
      QuerySensor(sid, true);
      return;
    }
  }

  ws->sendContent("</body></html>");
  ws->chunkedResponseFinalize();
}

static void handleConfig() {
}

static void handleJsonQuery() {
  const char *uri = ws->uri().c_str();
  // Serial.printf("%s(%s)\n", __FUNCTION__, uri);

  if (! ws->chunkedResponseModeStart(200, "text/json")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    // Serial.printf("%s: want HTTP/1.1", __FUNCTION__);
    return;
  }
  ws->sendContent("{");

  // Query everything
  if (uri == 0 || uri[1] == 0) {
    QuerySensors(false);
    return;
  }

  // Query for an individual sensor
  for (int sid=0; sid<MAX_SENSORS; sid++) {
    const char *sn = control->getSensorName(sid);
    if (sn == 0) {
      // Serial.printf("%s: checking sensor %d {null}\n", __FUNCTION__, sid);
      continue;
    }
    // Serial.printf("%s: checking sensor %d {%s}\n", __FUNCTION__, sid, sn);
    if (strcmp(sn, uri+6) == 0) {	// Prefixed by "/json/"
      QuerySensor(sid, false);
      return;
    }
  }

  ws->sendContent("}\n");
  ws->chunkedResponseFinalize();
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

void ws_reg() {
    // String user = ws->pathArg(0);
    // String device = ws->pathArg(1);
    // ws->send(200, "text/plain", "User: '" + user + "' and Device: '" + device + "'");
    // Serial.printf("User: '%s' device '%s'\n", user, device);
  Serial.printf("/config (uri %s) ... ", ws->uri().c_str());
  for (uint8_t i = 0; i < ws->args(); i++) {
    Serial.printf("%s %s, ", ws->argName(i), ws->arg(i));
  }
  Serial.printf("\n");
}

static void listSensors() {
  const char *uri = ws->uri().c_str();

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
	"<body><h1>ESP8266 Web Server</h1>\n"
	"\n\n<ul>\n");

  // Query for an individual sensor
  for (int sid=0; sid<MAX_SENSORS; sid++) {
    const char *sn = control->getSensorName(sid);
    if (sn == 0)
      continue;

    char line[80];
    sprintf(line, "<li><a href=\"%s\">%s</a>", sn, sn);
    ws->sendContent(line);
  }

  ws->sendContent("</ul></body></html>");
  ws->chunkedResponseFinalize();
}

void ws_begin() {
  ws = new ESP8266WebServer(80);
  
  ws->onNotFound(handleNotFound);
  ws->on("/config", handleConfig);
  ws->on("/sensors", listSensors);

  // Root JSON and HTML handlers
  ws->on("/", handleHtmlQuery);
  ws->on("/json", handleJsonQuery);

  // Per sensor handlers for HTML and JSON
  for (int sid=0; sid<MAX_SENSORS; sid++) {
    const char *sn = control->getSensorName(sid);
    if (sn == 0)
      continue;
    char s[32];

    sprintf(s, "/json/%s", sn);
    ws->on(s, handleJsonQuery);

    sprintf(s, "/%s", sn);
    ws->on(s, handleHtmlQuery);
  }

  // ws->on(UriRegex("^\\/users\\/([0-9]+)\\/devices\\/([0-9]+)$"), ws_reg);
  UriRegex r = UriRegex("^\\/config\\/.*$");
  ws->on(r, ws_reg);

  ws->begin();
  Serial.printf("Web server started, port %d\n", 80);
}

void ws_loop() {
  if (ws)
    ws->handleClient();
}
