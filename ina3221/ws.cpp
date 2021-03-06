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
#include <html.h>
#include <Control.h>

#include <uri/UriRegex.h>

ESP8266WebServer	*ws = 0;

// DEBUG
#define	SEND(x)	ws->sendContent(x)
// #define	SEND(x)	{ Serial.printf("%s", x); ws->sendContent(x); }

static void QuerySensor(int sid, bool html) {
    // Serial.printf("QuerySensor(%d,%s)\n", sid, html ? "html" : "json");
    const char *sn = control->getSensorName(sid);

    if (sn == 0)
      return;

    char line[80];
    if (html)
      sprintf(line, webpage_sensor_head_format, sn, sn);
    else
      sprintf(line, "{\"sensor\": \"%s\", ", sn);
    SEND(line);

    if (html) {
      SEND("<td>timestamp</td>\n");
    }
    for (int i=0; i<MAX_FIELDS; i++) {
      const char *fn = control->getFieldName(sid, i);
      if (fn) {
	if (html) {
          sprintf(line, "<td>%s</td>", fn);
	  SEND(line);
	}
      }
    }
    if (html) {
      SEND("</tr>\n");
    } else
      SEND("\"values\": [\n");

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
      SEND(line);
    }
    if (html) {
      SEND("</table>\n");
    } else
      SEND("]}\n");
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

/*
 * Show the web server configuration page.
 * This call should be between chunkedResponseModeStart() and chunkedResponseFinalize().
 */
static void showConfigurationPage() {
  ws->sendContent(webpage_general_head);
  ws->sendContent(webpage_configure_head);
  ws->sendContent(webpage_trigger_head);
  for (int i=0; i<MAX_TRIGGERS; i++) {
    ws->sendContent("<tr>");
    control->describeTrigger(ws, i);
    ws->sendContent("</tr>");
  }
  ws->sendContent(webpage_trigger_trail);
  ws->sendContent(webpage_stopper_head);
  for (int i=0; i<MAX_TRIGGERS; i++) {
    ws->sendContent("<tr>");
    control->describeStopper(ws, i);
    ws->sendContent("</tr>");
  }
  ws->sendContent(webpage_stopper_trail);
  ws->sendContent(webpage_configure_trail);
  ws->sendContent(webpage_general_trail);
}

/*
 * Show the web server main page.
 * This call should be between chunkedResponseModeStart() and chunkedResponseFinalize().
 */
static void showMainPage() {
  const char *uri = ws->uri().c_str();

  ws->sendContent(webpage_general_head);
  ws->sendContent(webpage_main_head);

  ws->sendContent("<h1>Sensor list</h1><p><form>");

  // Buttons to go to a sensor subpage
  for (int i=0; i<MAX_SENSORS; i++)
    if (control->getSensorName(i)) {
      char line[80];
      sprintf(line, "  <input type=\"submit\" name=\"button\" value=\"%s\" />\n", control->getSensorName(i));
      ws->sendContent(line);
    }

  ws->sendContent("<p>");

  // Buttons to export data
  for (int i=0; i<MAX_SENSORS; i++)
    if (control->getSensorName(i)) {
      char line[80];
      sprintf(line, "  <input type=\"submit\" name=\"get-json\" value=\"Export %s\" />\n", control->getSensorName(i));
      ws->sendContent(line);
    }

  ws->sendContent("</form>");

  ws->sendContent(webpage_main_trail);
  ws->sendContent(webpage_general_trail);
}

/*
 * This handles a query on the / root URI
 * That includes queries such as triggered by one of our buttons, e.g. http://measure.local/?button=Configure
 *  Specifically, this translates into ws->argName(0) = "button", ws->args(1) = "Configure".
 *  Or http://measure.local/?triggertypes-0=Min&sensors-0=AHT10+-+humidity&triggertypes-1=none&sensors-1=AHT10+-+temperature&stoppertypes-0=none&stoppertypes-1=none&button=Save
 */
static void handleHtmlQuery() {
  const char *uri = ws->uri().c_str();

  bool	in_configure = false,
  	configure_save = false,
	start_button = false,
	stop_button = false;

  /*
   * Do some analysis
   */
  // JSON export buttons
  bool sensor_page = false;
  uint8_t sid;
  if (ws->args() > 0) {
    for (uint8_t i = 0; i < ws->args(); i++)
      if (strcmp(ws->argName(i).c_str(), "get-json") == 0  && strcmp(ws->arg(i).c_str() + 7, control->getSensorName(i)) == 0) {
        sensor_page = true;
	sid = i;
      }
  }
  if (sensor_page) {
    if (! ws->chunkedResponseModeStart(200, "text/json")) {
      ws->send(500, "Want HTTP/1.1 for chunked responses");
      return;
    }
    QuerySensor(sid, false);
    ws->chunkedResponseFinalize();
    return;
  }

  // Serial.printf("%s: uri %s args %d\n", __FUNCTION__, uri, ws->args());
  if (ws->args() > 0) {
    for (uint8_t i = 0; i < ws->args(); i++) {
      // Serial.printf("\targ %d : %s - %s\n", i, ws->argName(i).c_str(), ws->arg(i).c_str());
      if (strcmp(ws->arg(i).c_str(), "Configure") == 0)
        in_configure = true;
      if (strcmp(ws->argName(i).c_str(), "button") == 0 && strcmp(ws->arg(i).c_str(), "Save") == 0)
        configure_save = true;
      if (strcmp(ws->argName(i).c_str(), "button") == 0 && strcmp(ws->arg(i).c_str(), "Start") == 0)
        start_button = true;
      if (strcmp(ws->argName(i).c_str(), "button") == 0 && strcmp(ws->arg(i).c_str(), "Stop") == 0)
        stop_button = true;
    }
  }

  if (! ws->chunkedResponseModeStart(200, "text/html")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    return;
  }

  /*
   * After this, we should perform business logic, and decide which page to show next.
   */

  // Trap per sensor pages first
  sensor_page = false;
  if (ws->args() >= 1 && strcasecmp(ws->argName(0).c_str(), "button") == 0) {
    for (int i=0; i<MAX_SENSORS; i++)
      if (control->getSensorName(i) && strcmp(ws->arg(0).c_str(), control->getSensorName(i)) == 0) {
        sensor_page = true;
	sid = i;
      }
  }

  if (sensor_page) {
    // Show it
    ws->sendContent(webpage_general_head);
    QuerySensor(sid, true);
    ws->sendContent(webpage_general_trail);
  } else {
    // Other stuff
    if (configure_save) {
      control->SaveConfiguration(ws);
      in_configure = true;
    }

    // Start button
    if (start_button)
      control->Start();

    // Stop button
    if (stop_button)
      control->Stop();

    if (in_configure) {
      showConfigurationPage();
    } else {
      showMainPage();
    }
  }

  // The end : wrap it up
  ws->chunkedResponseFinalize();
}

static void handleStart() {
  const char *uri = ws->uri().c_str();

  control->Start();

  if (! ws->chunkedResponseModeStart(200, "text/html")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    return;
  }
  
  // FIXME
  ws->sendContent(webpage_general_head);
  ws->sendContent(webpage_main_head);

  ws->sendContent(webpage_main_trail);
  ws->chunkedResponseFinalize();
}

static void handleStop() {
  const char *uri = ws->uri().c_str();

  control->Stop();

  if (! ws->chunkedResponseModeStart(200, "text/html")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    return;
  }
  
  // FIXME
  ws->sendContent(webpage_general_head);
  ws->sendContent(webpage_main_head);

  ws->sendContent(webpage_main_trail);

  ws->chunkedResponseFinalize();
}

static void handleConfig() {
  if (! ws->chunkedResponseModeStart(200, "text/json")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    return;
  }
  const char *json = control->WriteConfig();
  ws->sendContent(json);
  free((void *)json);
  ws->chunkedResponseFinalize();
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

#if 0
static void handleRegex() {
    // String user = ws->pathArg(0);
    // String device = ws->pathArg(1);
    // ws->send(200, "text/plain", "User: '" + user + "' and Device: '" + device + "'");
    // Serial.printf("User: '%s' device '%s'\n", user, device);
  Serial.printf("/config (uri %s) ... ", ws->uri().c_str());
  for (uint8_t i = 0; i < ws->args(); i++) {
    Serial.printf("%s %s, ", ws->argName(i), ws->arg(i));
  }
  Serial.printf("\n");

  char *json = "{\"triggers\":[{\"trigger\":\"min\",\"sensor\":\"AHT10\",\"field\":\"humidity\",\"type\":\"float\",\"value\":1.23}],\"stoppers\":[]}";
  Serial.printf("ReadConfig(%s)\n", json);
  // control->ReadConfig(ws->uri().c_str() + 8);
  control->ReadConfig(json);
  Serial.printf("handleRegex done\n");
}
#endif

static void listSensors() {
  const char *uri = ws->uri().c_str();

  if (! ws->chunkedResponseModeStart(200, "text/html")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    return;
  }
  
  ws->sendContent(webpage_general_head);
  ws->sendContent("\n\n<ul>\n");

  // Query for an individual sensor
  for (int sid=0; sid<MAX_SENSORS; sid++) {
    const char *sn = control->getSensorName(sid);
    if (sn == 0)
      continue;

    char line[80];
    sprintf(line, "<li><a href=\"%s\">%s</a>", sn, sn);
    ws->sendContent(line);
  }

  ws->sendContent("</ul>");
  ws->sendContent(webpage_general_trail);
  ws->chunkedResponseFinalize();
}

static void handleStatus() {
  char line[120];

  if (! ws->chunkedResponseModeStart(200, "text/html")) {
    ws->send(500, "Want HTTP/1.1 for chunked responses");
    return;
  }

  ws->sendContent(webpage_general_head);
  sprintf(line, "IP address %s, gateway %s\n", ips.c_str(), gws.c_str());
  ws->sendContent(line);

  sprintf(line, "<br>Boot at %s, ", timestamp(boot_time));
  int l = strlen(line);
  time_t now = time(0);
  sprintf(line + l, "time is now %s\n", timestamp(now));
  ws->sendContent(line);

  ws->sendContent(webpage_general_trail);
  ws->chunkedResponseFinalize();
}

void ws_begin() {
  ws = new ESP8266WebServer(80);
  
  ws->onNotFound(handleNotFound);
  ws->on("/config", handleConfig);
  ws->on("/sensors", listSensors);
  ws->on("/status", handleStatus);

  // Root JSON and HTML handlers
  ws->on("/", handleHtmlQuery);		// Also dispatches the buttons on our web pages, e.g. http://measure.local/?button=Configure
  ws->on("/json", handleJsonQuery);

  // Easy access for triggering remotely
  ws->on("/start", handleStart);
  ws->on("/stop", handleStop);

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

  // Not sure if we need this
  // UriRegex r = UriRegex("^\\/config\\/.*$");
  // ws->on(r, handleRegex);

  ws->begin();
  Serial.printf("Web server started, port %d\n", 80);
}

void ws_loop() {
  if (ws)
    ws->handleClient();
}
