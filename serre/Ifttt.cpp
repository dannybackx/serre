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
#include "Ifttt.h"
#include "global.h"

Ifttt::Ifttt() {
}

Ifttt::Ifttt(WiFiClient client) {
  this->client = client;
}

Ifttt::~Ifttt() {
}

extern "C" {
  void Debug(char *);
  void DebugInt(int);
}

void Ifttt::sendEvent(char *key, char *event, char *value1) {
  char json[256];
  if (value1 == NULL) {
    json[0] = 0;
  } else {
    sprintf(json, json_template1, value1);
  }
  sendEvent(key, event, value1, NULL, NULL);
}

const char *Ifttt::json_template1 = "{\"value1\":\"%s\"}";
const char *Ifttt::json_template2 = "{\"value1\":\"%s\",\"value2\":\"%s\"}";
const char *Ifttt::json_template3 = "{\"value1\":\"%s\",\"value2\":\"%s\",\"value3\":\"%s\"}";

void Ifttt::sendEvent(char *key, char *event, char *value1, char *value2) {
  char json[256];
  sprintf(json, json_template2, value1, value2);
  sendEventJson(key, event, json);
}

void Ifttt::sendEvent(char *key, char *event) {
  sendEventJson(key, event, (char *)"");
}

void Ifttt::sendEvent(char *key, char *event, char *value1, char *value2, char *value3) {
  char json[256];
  sprintf(json, json_template2, value1, value2, value3);
  sendEventJson(key, event, json);
}

void Ifttt::sendEventJson(char *key, char *event, char *json) {
  client.connect("maker.ifttt.com", 80);
  char post[256];	// big enough ?

  int len = strlen(json);

  const char *html_template = "POST /trigger/%s/with/key/%s HTTP/1.1\r\n"
  	"Host: maker.ifttt.com\r\n"
  	"Content-Type: application/json\r\n"
	"Content-Length: %d\r\n\r\n";
  sprintf(post, html_template, event, key, len);

  client.print(post);
  client.print(json);
  // client.stop();
}
