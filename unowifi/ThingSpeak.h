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
#ifndef _INCLUDE_THINGSPEAK_H_
#define _INCLUDE_THINGSPEAK_H_

#include "item.h"

class ThingSpeak {
public:
  ThingSpeak();
  ~ThingSpeak();
  void loop(int hr, int min);
  void sendEvent(char *key, char *event, char *value1);
  void sendEvent(char *key, char *event, char *value1, char *value2);
  void sendEvent(char *key, char *event, char *value1, char *value2, char *value3);
  void sendEvent(char *key, char *event);

private:
  int lasttime;
  REST *rest;
  static const char *json_template1, *json_template2, *json_template3, *html_template;
  void sendEventJson(char *key, char *event, char *json);
};
#endif
