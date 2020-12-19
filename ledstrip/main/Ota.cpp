/*
 * This module implements a small web server, with specific stuff to implement OTA.
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
#include "App.h"

#include "Ota.h"
#include "Mqtt.h"
#include "secrets.h"
#include "esp_log.h"

#include "Network.h"
#include <sys/socket.h>
#include "esp_ota_ops.h"

// Forward definitions of static functions
esp_err_t index_handler(httpd_req_t *req);
esp_err_t update_handler(httpd_req_t *req);
esp_err_t serverIndex_handler(httpd_req_t *req);
esp_err_t wildcard_handler(httpd_req_t *req);
esp_err_t WsNetworkConnected(void *ctx, system_event_t *event);
esp_err_t WsNetworkDisconnected(void *ctx, system_event_t *event);

const static char *swebserver_tag = "Ota";

Ota::Ota() {
  server = 0;
  network->RegisterModule(webserver_tag, WsNetworkConnected, WsNetworkDisconnected);
}

void Ota::Start() {
  int			sp = my_serverport;
  esp_err_t		err;
  httpd_config_t	cfg = HTTPD_DEFAULT_CONFIG();

  if (sp < 0)		// Only start if configured
    return;

  ESP_LOGD(webserver_tag, "Start webserver(%d)", cfg.server_port);

  cfg.server_port = sp;

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

  uri_hdl_def.uri = "/serverIndex";
  uri_hdl_def.handler = serverIndex_handler;
  httpd_register_uri_handler(server, &uri_hdl_def);

  uri_hdl_def.uri = "/update";
  uri_hdl_def.method = HTTP_POST;
  uri_hdl_def.handler = update_handler;
  httpd_register_uri_handler(server, &uri_hdl_def);
}

Ota::~Ota() {
  if (server)
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
 * Handler for browsing the caller's filesystem
 */
esp_err_t serverIndex_handler(httpd_req_t *req) {
  httpd_resp_send_chunk(req, ota->serverIndex, strlen(ota->serverIndex));

  // Terminate reply
  httpd_resp_send_chunk(req, ota->serverIndex, 0);
  // ota->SendPage(req);
  return ESP_OK;
}

static char *memmem(char *haystack, int hsl, char *needle, int nl) {
  for (int i=0; i<hsl - nl + 1; i++)
    if (strncmp(haystack+i, needle, nl) == 0)
      return haystack+i;
  return 0;
}

/*
 * OTA
 */
esp_err_t update_handler(httpd_req_t *req) {
  // int buflen;
  // buflen = httpd_req_get_url_query_len(req);
  // ESP_LOGI(swebserver_tag, "%s - URI {%s}, httpd_req_get_url_query_len() -> %d", __FUNCTION__, req->uri, buflen);
  // return ESP_OK;

  // Check whether this socket is secure.
  int sock = httpd_req_to_sockfd(req);

  if (! ota->isPeerSecure(sock)) {
    const char *reply = "Error: not authorized";
    httpd_resp_send(req, reply, strlen(reply));
    httpd_resp_send_500(req);
    return ESP_OK;
  }

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_handle_t update_handle = 0;

  if (configured != running) {
    ESP_LOGE(swebserver_tag, "Configured != running --> fix this first");
    return ESP_FAIL;
  }

  size_t vl;
  const char *var = "Content-Length";
  vl = httpd_req_get_hdr_value_len(req, var);
  if (vl != 0) {
    char *vv = (char *)malloc(vl + 3);
    httpd_req_get_hdr_value_str(req, var, vv, vl+2);
    ESP_LOGD(swebserver_tag, "%s : %s", var, vv);
    free((void *)vv);
  }

  char *boundary = 0;
  var = "Content-Type";
  vl = httpd_req_get_hdr_value_len(req, var);
  if (vl != 0) {
    char *vv = (char *)malloc(vl + 3);
    httpd_req_get_hdr_value_str(req, var, vv, vl+2);
    ESP_LOGD(swebserver_tag, "%s : %s", var, vv);

    boundary = strstr(vv, "boundary=");
    if (boundary != 0) {
      char *p = boundary + 9;
      int len = strlen(p);
      boundary = (char *)malloc(len+1);
      strcpy(boundary, p);
      ESP_LOGD(swebserver_tag, "Boundary {%s}", boundary);
    }

    free((void *)vv);
  }

  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
  char line[80];
  sprintf(line, "OTA : writing to partition subtype %d at offset 0x%x",
    update_partition->subtype, update_partition->address);
  ESP_LOGI(swebserver_tag, "%s", line);
  mqtt->Report(line);

  esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
  if (err != ESP_OK) {
    ESP_LOGE(swebserver_tag, "esp_ota_begin failed, %d %s", err, esp_err_to_name(err));
    return ESP_FAIL;
  }

  int		buflen = 1560;	// Larger than official Ethernet MTU, this should be a good start
  char		*buf = (char *)malloc(buflen + 1);
  int		ret;

  int		remain = req->content_len;
  int		offset = 0;

  ESP_LOGD(swebserver_tag, "Receiving (req content-len %d)", remain);
  while (remain > 0) {
    int len = remain;
    if (buflen < len)
      len = buflen;

    if ((ret = httpd_req_recv(req, buf, len)) <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        // retry
	continue;

      free(buf);
      sprintf(line, "OTA : httpd_req_recv failed, %d %s", ret, esp_err_to_name(ret));
      mqtt->Report(line);
      ESP_LOGE(swebserver_tag, "httpd_req_recv failed, %d %s", ret, esp_err_to_name(ret));
      return ESP_FAIL;	// Fail in other cases than timeout
    }

    // Skip initial headers, until 2x CRLR (0x0D 0x0A 0x0D 0x0A)
    char *ptr = buf;
    if (offset == 0) {
      int i;
      for (i=0; i<ret-3; i++) {
        if (buf[i] == 0x0D && buf[i+1] == 0x0A && buf[i+2] == 0x0D && buf[i+3] == 0x0A) {
	  ptr = &buf[i+4];
	  ret -= i+4;

	  // Start counting from here
	  remain = req->content_len;
	  remain += 500;	// HACK

	  break;
	}
      }
    }

    ESP_LOGD(swebserver_tag, "Received %d, remain %d", ret, remain-ret);

    int pos = 0;
    if (boundary && (remain < 10000)) {
      ESP_LOGE(swebserver_tag, "Checking boundary (offset %d) ..", offset);

      char *p = memmem(ptr, ret, boundary, strlen(boundary));
      if (p != 0) {
        pos = p - ptr;
	if (0 < pos && pos < ret) {
	  ESP_LOGE(swebserver_tag, "End of message at offset %d, %d", offset, pos);
	  ret = pos - 2;	// Remove CR-LF that precedes the boundary

	  // Write last piece
	  err = esp_ota_write(update_handle, ptr, ret);
	  if (err != ESP_OK) {
	    ESP_LOGE(swebserver_tag, "Failed to write OTA, %d %s", err, esp_err_to_name(err));
	    free(buf);
	    if (boundary)
	      free((void *)boundary);
	    return ESP_FAIL;
	  }

	  remain -= ret;
	  offset += ret;

	  break;
	}
      }
    }

    remain -= ret;
    offset += ret;

    err = esp_ota_write(update_handle, ptr, ret);
    if (err != ESP_OK) {
      ESP_LOGE(swebserver_tag, "Failed to write OTA, %d %s", err, esp_err_to_name(err));
      free(buf);
      if (boundary)
        free((void *)boundary);
      return ESP_FAIL;
    }

    if (pos > 0)
      break;

    vTaskDelay(1);
  }

  ESP_LOGI(swebserver_tag, "Bytes received : %d", offset);

  httpd_resp_send_chunk(req, NULL, 0);
  free(buf);
  if (boundary)
    free((void *)boundary);

  err = esp_ota_end(update_handle);
  if (err != ESP_OK) {
    ESP_LOGE(swebserver_tag, "OTA failed, %d %s", err, esp_err_to_name(err));
    return ESP_FAIL;
  }
  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK) {
    ESP_LOGE(swebserver_tag, "OTA set bootable failed, %d %s", err, esp_err_to_name(err));
    return ESP_FAIL;
  }

  mqtt->Report("OTA success");
  vTaskDelay(500);

  esp_restart();
  return ESP_OK;
}

/*
 * Used by handlers after their processing, to send a normal page back to the user.
 * No status or error codes called.
 */
void Ota::SendPage(httpd_req_t *req) {
  ESP_LOGI(swebserver_tag, "%s", __FUNCTION__);

  // No response code set, assumption that this is a succesfull call
  httpd_resp_send_chunk(req, loginIndex, strlen(loginIndex));
  // httpd_resp_send_chunk(req, reply_template2, strlen(reply_template2));

  // Terminate reply
  httpd_resp_send_chunk(req, loginIndex, 0);
}

/*
 * This gets the standard initial request, just http://this-node
 */
esp_err_t index_handler(httpd_req_t *req) {
  // Check whether this socket is secure.
  int sock = httpd_req_to_sockfd(req);

  if (! ota->isPeerSecure(sock)) {
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

  ota->SendPage(req);
  return ESP_OK;
}

/*
 * Expose the server handle so we can pass it to the ACME library
 */
httpd_handle_t Ota::getServer() {
  return server;
}

esp_err_t WsNetworkConnected(void *ctx, system_event_t *event) {
  ESP_LOGI(swebserver_tag, "Starting Ota");

  ota->Start();
#ifdef USE_ACME
  if (acme) acme->setOta(ota->getServer());
#endif
  return ESP_OK;
}

esp_err_t WsNetworkDisconnected(void *ctx, system_event_t *event) {
  if (ota->getServer())
    httpd_stop(ota->getServer());
  return ESP_OK;
}

bool Ota::isPeerSecure(int sock) {
  struct sockaddr_in sa;
  socklen_t al = sizeof(sa);

  errno = 0;
  if (getpeername(sock, (struct sockaddr *)&sa, &al) != 0) {
    ESP_LOGE(webserver_tag, "%s: getpeername failed, errno %d", __FUNCTION__, errno);
    return false;
  }

  if (sa.sin_addr.s_addr == 0) {
    // Try ipv6, see https://www.esp32.com/viewtopic.php?t=8317
    struct sockaddr_in6 sa6;
    al = sizeof(sa6);
    if (getpeername(sock, (struct sockaddr *)&sa6, &al) != 0) {
      ESP_LOGE(webserver_tag, "%s: getpeername6 failed, errno %d", __FUNCTION__, errno);
      return false;
    }

    sa.sin_addr.s_addr = sa6.sin6_addr.un.u32_addr[3];
  }

  ESP_LOGD(webserver_tag, "%s: IP address is %s, errno %d", __FUNCTION__, inet_ntoa(sa.sin_addr), errno);

  // FIXME
  // return isIPSecure(&sa);
  return true;
}

const char *Ota::loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
const char *Ota::serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";
