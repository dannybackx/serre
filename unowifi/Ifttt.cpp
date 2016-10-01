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
 *
 * This contains code to send a message to IFTTT
 *
 * To trigger an Event
 *
 * Make a POST or GET web request to:
 * https://maker.ifttt.com/trigger/{event}/with/key/{key}
 * With an optional JSON body of:
 * { "value1" : "", "value2" : "", "value3" : "" }
 * The data is completely optional, and you can also pass value1, value2, and value3
 * as query parameters or form variables. This content will be passed on to the Action
 * in your Recipe.
 * You can also try it with curl from a command line.
 * curl -X POST https://maker.ifttt.com/trigger/{event}/with/key/{key}
 */
#include <Arduino.h>
#include <ArduinoWiFi.h>
#include "SFE_BMP180.h"
#include "global.h"
#include "Ifttt.h"

Ifttt::Ifttt() {
}

#ifdef ESP8266
Ifttt::Ifttt(WiFiClient client) {
  this->client = client;
}
#endif

Ifttt::~Ifttt() {
}

const char *Ifttt::json_template1 = "{\"value1\":\"%s\"}";
const char *Ifttt::json_template2 = "{\"value1\":\"%s\",\"value2\":\"%s\"}";
const char *Ifttt::json_template3 = "{\"value1\":\"%s\",\"value2\":\"%s\",\"value3\":\"%s\"}";
const char *Ifttt::html_template =
	"POST /trigger/%s/with/key/%s HTTP/1.1\r\n"
  	"Host: maker.ifttt.com\r\n"
  	"Content-Type: application/json\r\n"
	"Content-Length: %d\r\n\r\n";

void Ifttt::sendEvent(char *key, char *event) {
  sendEventJson(key, event, (char *)"");
}

void Ifttt::sendEvent(char *key, char *event, char *value1) {
  if (value1 == 0)
    return sendEvent(key, event);
  char *json = (char *)malloc(strlen(json_template1) + strlen(value1) + 5);
  sprintf(json, json_template1, value1);
  sendEventJson(key, event, json);
  free(json);
}

void Ifttt::sendEvent(char *key, char *event, char *value1, char *value2) {
  if (value2 == 0)
    return sendEvent(key, event, value1);
  char *json = (char *)malloc(strlen(json_template2) + strlen(value1) + strlen(value2) + 5);
  sprintf(json, json_template2, value1, value2);
  sendEventJson(key, event, json);
  free(json);
}

void Ifttt::sendEvent(char *key, char *event, char *value1, char *value2, char *value3) {
  if (value3 == 0)
    return sendEvent(key, event, value1, value2);
  char *json = (char *)malloc(strlen(json_template3) + strlen(value1) + strlen(value2) + strlen(value3) + 5);
  sprintf(json, json_template3, value1, value2, value3);
  sendEventJson(key, event, json);
  free(json);
}

void Ifttt::sendEventJson(char *key, char *event, char *json) {
#ifdef ESP8266
  client.connect("maker.ifttt.com", 80);

  int len = strlen(json);

  char *post = (char *)malloc(strlen(html_template) + strlen(key) + strlen(event) + 5);
  sprintf(post, html_template, event, key, len);

  client.print(post);
  client.print(json);

  // Debug(post); Debug("\n");
  // Debug(json); Debug("\n");

  // client.stop();

  free(post);
#endif
}
