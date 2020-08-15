/*
 * This module implements a small web server
 * Please make sure to protect access to this module, it still needs to be secured.
 *
 * FIXME Awaiting esp-idf 3.3 or 4.0 which offers a https service...
 *
 * Copyright (c) 2019, 2020 Danny Backx
 *
 * License (GNU Lesser General Public License) :
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 3 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Arduino.h>
#include "WebServer.h"
#include "secrets.h"
#include "esp_log.h"

#include "Kippen.h"
#include "Network.h"
#include "Secure.h"

// Forward definitions of static functions
esp_err_t alarm_handler(httpd_req_t *req);
esp_err_t index_handler(httpd_req_t *req);
esp_err_t wildcard_handler(httpd_req_t *req);

const static char *swebserver_tag = "WebServer static";

WebServer::WebServer() {
  esp_err_t		err;
  httpd_config_t	cfg = HTTPD_DEFAULT_CONFIG();
  int			sp = CONFIG_WEBSERVER_PORT;

  if (sp < 0)		// Only start if configured
    return;

  cfg.server_port = sp;

  ESP_LOGD(webserver_tag, "Start webserver(%d)", cfg.server_port);

  if ((err = httpd_start(&server, &cfg)) != ESP_OK) {
    ESP_LOGE(webserver_tag, "failed to start %s (%d)", esp_err_to_name(err), err);
    return;
  }

  // Handle the default query, this is "/", not "/index.html".
  httpd_uri_t uri_hdl_def = {
    "/",			// URI handled
    HTTP_GET,			// HTTP method
    index_handler,		// Handler
    (void *)0			// User context
  };
  httpd_register_uri_handler(server, &uri_hdl_def);

#if defined(IDF_VER) && (IDF_MAJOR_VERSION > 3 || IDF_MINOR_VERSION > 2)
  // Only available in esp-idf 3.3 and up
  cfg.uri_match_fn = httpd_uri_match_wildcard;
#endif

  uri_hdl_def.uri = "/*";
  uri_hdl_def.handler = wildcard_handler;
  httpd_register_uri_handler(server, &uri_hdl_def);

  // Handler for arming from a browser
  uri_hdl_def.uri = "/alarm";
  uri_hdl_def.handler = alarm_handler;
  httpd_register_uri_handler(server, &uri_hdl_def);

  return;
}

WebServer::~WebServer() {
  httpd_stop(server);
}

/*
 * URI Handlers
 */

/*
 * Generic handler
 * No conditional compilation here but only called if esp-idf version is right
 */
esp_err_t wildcard_handler(httpd_req_t *req) {
  return ESP_OK;
}

/*
 * Handler for requests to change the alarm armed state
 *
 * FIX ME
 * Security code required ;-)
 */
esp_err_t alarm_handler(httpd_req_t *req) {
  // Check whether this socket is secure.
  int sock = httpd_req_to_sockfd(req);

  if (! security->isPeerSecure(sock)) {
    const char *reply = "Error: not authorized";
    httpd_resp_send(req, reply, strlen(reply));
    httpd_resp_send_500(req);
    return ESP_OK;
  }

#ifdef WEB_SERVER_IS_SECURE
  int	buflen;
  char	*buf;

  ESP_LOGI(swebserver_tag, "%s - URI {%s}", __FUNCTION__, req->uri);
  buflen = httpd_req_get_url_query_len(req);

  ESP_LOGI(swebserver_tag, "%s - httpd_req_get_url_query_len() => %d", __FUNCTION__, buflen);

  if (buflen == 0) {
    const char *reply = "Error: no parameters specified";
    httpd_resp_send(req, reply, strlen(reply));
    httpd_resp_send_500(req);
    return ESP_OK;
  }
  
  buf = (char *)malloc(buflen + 1);
  esp_err_t e;

  if ((e = httpd_req_get_url_query_str(req, buf, buflen + 1)) == ESP_OK) {
    ESP_LOGI(swebserver_tag, "%s found query => %s", __FUNCTION__, buf);
    char param[32];

    /* Get value of expected key from query string */
    if (httpd_query_key_value(buf, "armed", param, sizeof(param)) == ESP_OK) {
      ESP_LOGI(swebserver_tag, "Found URL query parameter => armed = \"%s\"", param);
    }
  } else {
    ESP_LOGE(swebserver_tag, "%s: could not get URL query, error %s %d",
      __FUNCTION__, esp_err_to_name(e), e);
    free(buf);
    const char *reply = "Could not get url query";
    httpd_resp_send(req, reply, strlen(reply));
    httpd_resp_send_500(req);
    return ESP_OK;
  }
  free(buf);

  ws->SendPage(req);
#endif
  return ESP_OK;
}

/*
 * Used by handlers after their processing, to send a normal page back to the user.
 * No status or error codes called.
 */
void WebServer::SendPage(httpd_req_t *req) {
  ESP_LOGI(swebserver_tag, "%s", __FUNCTION__);

  // Reply
  const char *reply_template1 =
    "<!DOCTYPE html>"
    "<HTML>"
    "<TITLE>ESP32 %s controller</TITLE>\r\n"
    "<BODY>"
    "<H1>General</h1>\r\n"
    "<p>Node name ss"
    "<p>Time %s"
    "<H1>Environment</H1>\r\n"
    "<p>Weather dd"
    "<p>Temperature cc&deg;C"
    "<p>Pressure pp mb"
    "<p>Wind ww km/h\r\n";
  const char *reply_template2 =
    "<P>\r\n"
#ifdef WEB_SERVER_IS_SECURE
    "<form action=\"/alarm\" method=\"get\">\r\n"
      "<button class=\"button\" type=\"submit\" name=\"armed\" value=\"uit\">Uit</button>\r\n"
      "<button class=\"button\" type=\"submit\" name=\"armed\" value=\"nacht\">Nacht</button>\r\n"
      "<button class=\"button\" type=\"submit\" name=\"armed\" value=\"aan\">Aan</button>\r\n"
    "</form>"
#endif
    "</P>\r\n"
    "</BODY>"
    "</HTML>";

  char *buf1 = (char *)malloc(strlen(reply_template1) + 50);
  char *buf2 = (char *)malloc(strlen(reply_template1) + 70);
  
  char ts[20];
  struct timeval tv;
  gettimeofday(&tv, 0);
  struct tm *now = localtime(&tv.tv_sec);
  strftime(ts, sizeof(ts)-1, "%F %R", now);

  sprintf(buf2, reply_template1, "kippen", ts);
#if 0
  weather->strfweather(buf1, 250, reply_template1);
  sprintf(buf2, buf1,
    "kippen",		// Node name, in the title
    "kippen",		// Node name, in page body
    ts				// time
    );
#endif
  // No response code set, assumption that this is a succesfull call
  httpd_resp_send_chunk(req, buf2, strlen(buf2));
  free(buf1);
  free(buf2);

  httpd_resp_send_chunk(req, reply_template2, strlen(reply_template2));

  // Terminate reply
  httpd_resp_send_chunk(req, reply_template2, 0);
}

/*
 * This gets the standard initial request, just http://this-node
 */
esp_err_t index_handler(httpd_req_t *req) {
  // Check whether this socket is secure.
  int sock = httpd_req_to_sockfd(req);

  if (! security->isPeerSecure(sock)) {
    const char *reply = "Error: not authorized";
    httpd_resp_send(req, reply, strlen(reply));
    
    struct sockaddr_in6 sa6;
    socklen_t salen = sizeof(sa6);
    if (getpeername(sock, (sockaddr *)&sa6, &salen) == 0) {
      struct sockaddr_in sa;
      sa.sin_addr.s_addr = sa6.sin6_addr.un.u32_addr[3];
      ESP_LOGE(swebserver_tag, "%s: access attempt for %s from %s, not allowed",
        __FUNCTION__, req->uri, inet_ntoa(sa.sin_addr));
    } else {
      ESP_LOGE(swebserver_tag, "%s: access attempt for %s, not allowed", __FUNCTION__, req->uri);
    }

    httpd_resp_set_status(req, "401 Not authorized");
    return ESP_OK;
  }

  ws->SendPage(req);
  return ESP_OK;
}

/*
 * Expose the server handle so we can pass it to the ACME library
 */
httpd_handle_t WebServer::getServer() {
  return server;
}
