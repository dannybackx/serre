/*
 * Copyright (c) 2016, 2017 Danny Backx
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
 * This contains code to send a message to a dynamic dns service such as no-ip.com
 * to register the IP address of this device.
 * The goal is that devices get a fixed DNS name such as esp-device-xxahg.no-ip.com, determined
 * by the owner of the device, and authomatically attach their current IP address (usually variable,
 * depending on the setup of the network they're in) to it.
 *
 * From https://www.noip.com/integrate/request :
 *
 * An example update request string
 *	http://username:password@dynupdate.no-ip.com/nic/update?hostname=mytest.testdomain.com&myip=1.2.3.4
 *
 * An example basic, raw HTTP header GET request
 *	GET /nic/update?hostname=mytest.testdomain.com&myip=1.2.3.4 HTTP/1.0
 *	Host: dynupdate.no-ip.com
 *	Authorization: Basic base64-encoded-auth-string
 *	User-Agent: Bobs Update Client WindowsXP/1.2 bob@somedomain.com
 *
 * The base-encoded-auth-string can be created by using the base64 command and entering
 *	userid:password
 * on a single line. For instance :
 *	acer: {1} base64 
 *	userid:password			<-- substitute your own
 *	dXNlcmlkOnBhc3N3b3JkCg==	<-- this output is what you need
 *	acer: {2} 
 */
#include <Arduino.h>

#include <ELClient.h>
#include <ELClientRest.h>

#include "Dyndns.h"

extern ELClient esp;

const char *Dyndns::get_template1 = "/nic/update?hostname=%s";
const char *Dyndns::get_template2 = "/nic/update?hostname=%s&myip=%s";
const char *Dyndns::hdr_template = "Authorization: Basic %s\r\n";

Dyndns::Dyndns() {
  url = "dynupdate.no-ip.com";		// Default, can be overruled by method

  rest = new ELClientRest(&esp);
  hostname = ip = auth = 0;
  buffer = 0;
}

Dyndns::~Dyndns() {
  if (rest) {
    delete rest;
    rest = 0;
  }
}

void Dyndns::setHostname(const char *hostname) {
  this->hostname = (char *)hostname;
}

void Dyndns::setAddress(const char *ip) {
  this->ip = (char *)ip;
}

void Dyndns::setAuth(const char *auth) {
  this->auth = (char *)auth;
}

void Dyndns::setUrl(const char *url) {
  this->url = (char *)url;
}

void Dyndns::update() {
  char *msg1, *msg2;
  int err = rest->begin(url);
  if (err != 0) {
    delete rest;
    rest = 0;

    Serial.print("Dyndns : could not initialize ");
    Serial.print(url);
    Serial.print(", error code ");
    Serial.println(err);
    return;
  }

  if (ip != 0)
    msg1 = (char *)malloc(strlen(get_template2) + strlen(hostname) + strlen(ip) + 5);
  else
    msg1 = (char *)malloc(strlen(get_template1) + strlen(hostname) + 5);
  if (msg1 == 0) {
    Serial.println("Could not allocate memory for HTTP query");
    return;
  }

  if (ip != 0)
    sprintf(msg1, get_template2, hostname, ip);
  else
    sprintf(msg1, get_template1, hostname);
  // Serial.print("Query "); Serial.println(msg1);

  msg2 = (char *)malloc(strlen(hdr_template) + strlen(url) + strlen(auth) + 5);
  if (msg2 == 0) {
    Serial.println("Could not allocate memory for HTTP query");
    free(msg1);
    return;
  }
  sprintf(msg2, hdr_template, auth);
  // Serial.print("Header "); Serial.println(msg2);

  rest->setHeader(msg2);
  rest->setUserAgent("dannys esp-link library");
  rest->request(msg1, "GET", "", 0);

  buffer = (char *)malloc(buffer_size);
  if (buffer == 0) {
    Serial.println("Could not allocate memory for HTTP query");
    free(msg1); free(msg2);
    return;
  }
  buffer[0] = 0;

  err = rest->waitResponse(buffer, buffer_size);

  // Serial.print("Error code "); Serial.println(err); Serial.println(buffer);

  if (err == HTTP_STATUS_OK) {
    // Serial.println("Success ");
  } else if (err == 0) {
    // timeout
    Serial.println("Timeout");
  } else {
    Serial.println("HTTP GET failed");
  }
  free(msg1); free(msg2);
  free(buffer);
}
