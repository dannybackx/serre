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

const char *webpage_general_head =
  "<!DOCTYPE html><html>"
  "<head>\n"
  "";

const char *webpage_general_trail =
  "</body></html>\n";

const char *webpage_main_head =
  // Web Page Heading
  "<title>Measurement station web server</title>"
  "</head><body>\n"
  "<h1>Measurement station web server</h1>\n"
  /*
   * Include trial buttons
   */
  "<p><form>"
  "  <input type=\"submit\" name=\"button\" value=\"Submit\" />\n"
  "  <input type=\"submit\" name=\"button\" value=\"Start\" />\n"
  "  <input type=\"submit\" name=\"button\" value=\"Stop\" />\n"
  "  <input type=\"submit\" name=\"button\" value=\"Configure\" />\n"
  "</form>\n"
  "\n";

const char *webpage_main_trail =
  "</body></html>";

const char *webpage_configure_head =
  // Web Page Heading
  "<title>Measure - Configuration</title>"
  "</head><body>\n"
  "<h1>Measure - Configuration</h1>\n"

  "<p><form>"
  "  <input type=\"submit\" name=\"button\" value=\"Load\" />\n"
  "  <input type=\"submit\" name=\"button\" value=\"Save\" />\n"
  "  <input type=\"submit\" name=\"button\" value=\"Start\" />\n"
  "  <input type=\"submit\" name=\"button\" value=\"Stop\" />\n"
  "  <input type=\"submit\" name=\"button\" value=\"Back\" />\n"
  // "</form>\n"

  "\n";

const char *webpage_configure_trail =
  "</form>"
  "</body></html>";

const char *webpage_trigger_head =
  "<h2>Triggers</h2>\n"
  "<table border=\"1\">\n"
  // Top row
  "<tr><td>Number</td><td>Type</td><td>Sensor &amp; field</td><td>Min. value</td><td>Max. value</td></tr>\n"
  "";
const char *webpage_trigger_trail =
  "</table>";
const char *webpage_stopper_head =
  "<h2>Stoppers</h2>\n"
  "<table border=\"1\">\n"
  "<tr><td>Number</td><td>Type</td><td>Value</td></tr>\n"
  "";
const char *webpage_stopper_trail =
  "</table>";

const char *webpage_sensor_dropdown_start = "<select name=\"sensors\" id=\"sensors\">\n";
const char *webpage_sensor_dropdown_format = "  <option value=\"%s\">%s</option>\n";
const char *webpage_sensor_dropdown_format_selected = "  <option value=\"%s\" selected=\"selected\">%s</option>\n";
const char *webpage_sensor_dropdown_end = "</select>\n";

const char *webpage_triggertype_dropdown_start = "<select name=\"triggertypes\" id=\"triggertypes\">\n";
const char *webpage_triggertype_dropdown_format = "  <option value=\"%s\">%s</option>\n";
const char *webpage_triggertype_dropdown_format_selected = "  <option value=\"%s\" selected=\"selected\">%s</option>\n";
const char *webpage_triggertype_dropdown_end = "</select>\n";

const char *webpage_stopper_dropdown_start = "<select name=\"stoppertypes\" id=\"stoppertypes\">\n";
const char *webpage_stopper_dropdown_format = "  <option value=\"%s\">%s</option>\n";
const char *webpage_stopper_dropdown_format_selected = "  <option value=\"%s\" selected=\"selected\">%s</option>\n";
const char *webpage_stopper_dropdown_end = "</select>\n";

const char *boot_msg_format = "Boot, time set to %04d.%02d.%02d %02d:%02d:%02d\n";
const char *time_msg_format = "Time set to %04d.%02d.%02d %02d:%02d:%02d\n";
const char *timestamp_format = "%04d.%02d.%02d %02d:%02d:%02d";
