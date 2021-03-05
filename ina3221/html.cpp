/*
 * Web server for measurement station : HTML for the web server
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

const char *webpage_main =
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

  /*
   * Include trial buttons
   */
  "<p><form>"
  "  <input type=\"submit\" name=\"button\" value=\"Submit\" />"
  "  <input type=\"submit\" name=\"button\" value=\"Start\" />"
  "  <input type=\"submit\" name=\"button\" value=\"Stop\" />"
  "  <input type=\"submit\" name=\"button\" value=\"Configure\" />"
  "</form>\n"
  "\n";

const char *webpage_configure =
  "<!DOCTYPE html><html>"
  "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<link rel=\"icon\" href=\"data:,\">"

  // Web Page Heading
  "<title>ESP8266 Web Server</title>"
  "<body><h1>ESP8266 Web Server</h1>\n"

  "<p><form>"
  "  <input type=\"submit\" name=\"button\" value=\"Load\" />"
  "  <input type=\"submit\" name=\"button\" value=\"Save\" />"
  "  <input type=\"submit\" name=\"button\" value=\"Start\" />"
  "  <input type=\"submit\" name=\"button\" value=\"Stop\" />"
  "  <input type=\"submit\" name=\"button\" value=\"Back\" />"
  "</form>\n"

  "\n";

const char *webpage_trigger_head =
  "<h2>Triggers</h2>\n"
  "<table border=\"1\">\n"
  "";
const char *webpage_trigger_trail =
  "</table>";
const char *webpage_stopper_head =
  "<h2>Stoppers</h2>\n"
  "<table border=\"1\">\n"
  "";
const char *webpage_stopper_trail =
  "</table>";

const char *webpage_sensor_dropdown_start = "<select name=\"sensors\" id=\"sensors\">";
const char *webpage_sensor_dropdown_format = "  <option value=\"%s\">%s</option>";
const char *webpage_sensor_dropdown_format_selected = "  <option value=\"%s\" selected=\"selected\">%s</option>";
const char *webpage_sensor_dropdown_end = "</select>";

const char *webpage_triggertype_dropdown_start = "<select name=\"triggertypes\" id=\"triggertypes\">";
const char *webpage_triggertype_dropdown_format = "  <option value=\"%s\">%s</option>";
const char *webpage_triggertype_dropdown_format_selected = "  <option value=\"%s\" selected=\"selected\">%s</option>";
const char *webpage_triggertype_dropdown_end = "</select>";
