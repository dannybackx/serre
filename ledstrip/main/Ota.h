/*
 * This module implements a small web server, for OTA
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


#ifndef	_OTA_H_
#define	_OTA_H_

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_http_server.h>

class Ota {
  public:
    Ota();
    ~Ota();

  private:
    const char *webserver_tag = "Ota";
    void Start();

    httpd_handle_t	server;

    friend esp_err_t update_handler(httpd_req_t *req);
    friend esp_err_t WsNetworkConnected(void *ctx, system_event_t *event);
    friend esp_err_t WsNetworkDisconnected(void *ctx, system_event_t *event);

    char			*my_name;
    const int			my_serverport = 80;

    // 
    bool isPeerSecure(int sock);

    static const char *loginIndex;
    static const char *serverIndex;
};

extern Ota *ota;
#endif	/* _OTA_H_ */
