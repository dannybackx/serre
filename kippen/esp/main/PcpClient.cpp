/*
 * Copyright (c) 2017, 2020 Danny Backx
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
 * This class allows you to "punch a hole" in the NAT router/firewall typically used.
 *
 * A local port on the esp can be remapped to some other port number on the router.
 * Someone accessing that port number can thereby get access to the esp even through
 * the NAT router.
 *
 * Protocol used : PCP (Port Control Protocol), RFC 6887, which is a superset of
 * NAT-PMP (NAT Port Mapping Protocol), RFC 6886.
 *
 * Obviously (grin) this is a minimal implementation.
 */

#include <esp_log.h>
#include <string.h>
#include "PcpClient.h"
#include <netinet/in.h>

static PcpClient *self;
void pcp_task(void *);	// Forward declaration needed, as in the include file it's just a friend, not declared.

PcpClient::PcpClient() {
  sock = -1;
  self = this;
  task = 0;

  xTaskCreate(&pcp_task, "pcp mcast", 4096, NULL, 5, &task);
}

PcpClient::~PcpClient() {
  if (sock) {
    close(sock);
    sock = 0;
  }
  if (task) {
    vTaskDelete(task);
    task = 0;
  }
}

void PcpClient::setLocalIP(in_addr_t local) {
  ESP_LOGD("pcp", "SetLocalIP(%s)", inet_ntoa(local));

  this->local = local;
}

void PcpClient::setRouter(const char *hostname) {
  router_name = (char *)hostname;
  router_ip = inet_addr(hostname);
}

void PcpClient::setRouter(const in_addr_t host) {
  router_ip = host;
}

esp_err_t PcpClient::NetworkConnected(void *ctx, system_event_t *event) {
  switch (event->event_id) {
    case SYSTEM_EVENT_GOT_IP6:
      break;

    case SYSTEM_EVENT_STA_GOT_IP:
      setLocalIP(event->event_info.got_ip.ip_info.ip.addr);

      // pcp->setRouter("192.168.0.228");			// Acer
      pcp->setRouter("192.168.0.1");
      pcp->addPort(35777, 35777, IPPROTO_TCP, 1000);	// FIX ME test

      break;

    default:
      break;
  }
  
  return ESP_OK;
}

void PcpClient::sendPacket(const char *packet, const int len) {
  if (sock < 0) {
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    ESP_LOGD("pcp", "sendPacket: socket %d", sock);

    struct sockaddr_in ls;
    memset((char *)&ls, 0, sizeof(ls));
    ls.sin_family = PF_INET;
    ls.sin_port = htons(pcp_client_port);
    ls.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr *)&ls, sizeof(ls)) < 0) {
      ESP_LOGE("pcp", "sendPacket: bind to port %d failed %d %s", pcp_client_port, errno, strerror(errno));
    } else
      ESP_LOGD("pcp", "sendPacket: bind to port %d ok", pcp_client_port);

  }
#if 0
  {
    char s[64], a[10];
    for (int i=0; i<len; i++) {
      if (i % 16 == 0) {
	if (i > 0) {
	  ESP_LOGI("pcp", "send %s", s);
	  s[0]=0;
	}
        sprintf(s, "%02x : ", i);
      }
      sprintf(a, "%02x ", packet[i]);
      strcat(s, a);
    }
    if (s[0]) {
      ESP_LOGI("pcp", "send %s", s);
    }
  }
#endif
  struct sockaddr_in dest;
  dest.sin_family = AF_INET;
  dest.sin_port = ntohs(pcp_server_port);
  dest.sin_addr.s_addr = router_ip;

  ssize_t r = sendto(sock, packet, len, 0, (const sockaddr *)&dest, sizeof(dest));
  if (r <= 0)
    ESP_LOGE("pcp", "sendto -> %d (errno %d %s)", r, errno, strerror(errno));
  else
    ESP_LOGD("pcp", "sendto -> %d", r);
}

in_addr_t PcpClient::getRouterExternalAddress() {
  return external;
}

void PcpClient::addPort(int16_t localport, int16_t remoteport, int8_t protocol, int32_t lifetime) {
  ESP_LOGI("pcp", "PCP addPort %d %d proto %d lifetime %d", 0xFFFF & localport, 0xFFFF & remoteport, protocol, lifetime);

  struct PcpPacket	p;
  memset(&p, 0, sizeof(p));

  p.rh.version = 2;						// PCP
  p.rh.opcode = PCP_OPCODE_MAP;					// MAP = 1
  // p.rh.reserved = 0;
  // p.rh.result_code = 0;
  p.rh.lifetime = htonl(lifetime);
  p.rh.client_ip[0] = p.rh.client_ip[1] = 0;
  p.rh.client_ip[2] = htonl(0x0000FFFF);			// FIXME, looks like "IPv4 follows"
  p.rh.client_ip[3] = local;					// ESP

  p.mof.nonce.initialize();
  p.mof.protocol = protocol;					// e.g. IPPROTO_TCP; 0 = all protocols
  p.mof.reserved[0] = p.mof.reserved[1] = p.mof.reserved[2] = 0;
  p.mof.internal_port = htons(localport);
  p.mof.external_port = htons(remoteport);
  // 0 is ok as suggestion for external IP
  p.mof.external_ip[0] = p.mof.external_ip[1] = p.mof.external_ip[2] = p.mof.external_ip[3] = 0;

  sendPacket((const char *)&p, sizeof(p));

  PcpMappingInventory inv;
  inv.result_code = 0xFF;					// Our own code to indicate it's been requested
  inv.lifetime = lifetime;
  inv.nonce.copy(p.mof.nonce);
  inv.protocol = protocol;
  inv.internal_port = localport;
  inv.external_port = remoteport;
  inv.external_ip[0] = inv.external_ip[1] = inv.external_ip[2] = inv.external_ip[3] = 0;

  requests.push_front(inv);

  ESP_LOGE("pcp", "Requests list count %d", requests.size());
}

void PcpClient::deletePort(int16_t localport, int8_t protocol) {
  struct PcpPacket p;
  memset(&p, 0, sizeof(p));

  p.rh.lifetime = 0;			// Lifetime = 0 means delete

  sendPacket((const char *)&p, sizeof(p));
}

/* Note for later :
 * Syntax in a static member function :
 *  (self ->* ((PcpClient*)self)->PcpClient::catcher)(1, "xx");
 * Syntax in a member function :
 *  (this ->* ((PcpClient*)this)->PcpClient::catcher)(1, "xx");
 *
 * See also http://stackoverflow.com/questions/990625/c-function-pointer-class-member-to-non-static-member-function
 */
/*
 * This function is not static but a friend, to access private fields and methods in the class.
 *
 * Note that RFC 6887 describes a maximum of 1100 octets as a reply packet size.
 * We're counting on significantly less because we send small requests ourselves.
 * The union is here to avoid buffer overrun.
 */
void pcp_task(void *ptr) {
  union {
    struct PcpPacket	pcp;
    char buffer[128];		// Packets we receive should be smaller than this, see above. Usually 60.
  } rx;
  int count = 0;

  while (1) {
    struct sockaddr_in sender;
    socklen_t	sl = sizeof(sender);

    if (pcp == 0 || pcp->sock < 0) {
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      continue;
    }
    int len = recvfrom(pcp->sock, &rx, sizeof(rx), 0, (struct sockaddr *)&sender, &sl);
    if (len < 0) {
      ESP_LOGE("pcp", "Recvfrom failed, errno %d %s, socket %d", errno, strerror(errno), pcp->sock);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      if (count++ > 5)
        vTaskDelete(0);		// current task
      continue;
    } else {
      ESP_LOGD("pcp", "Received msg len %d", len);

    if ((rx.pcp.rh.opcode & 0x80) == 0) {
      ESP_LOGE("pcp", "Packet is not a reply, discarding");
      break;
    }
    switch (rx.pcp.rh.opcode & 0x7F) {
    case PCP_OPCODE_MAP:
      pcp->PcpReplyMapping(&rx.pcp);
      break;
    default:
      ESP_LOGE("pcp", "Received packet protocol %02x opcode %02x unknown", rx.pcp.rh.version, rx.pcp.rh.opcode);
      return;
    }
#if 1
  {
    char s[64], a[10];
    char *packet = (char *)&rx;
    for (int i=0; i<len; i++) {
      if (i % 16 == 0) {
	if (i > 0) {
	  ESP_LOGI("pcp", "receive %s", s);
	  s[0]=0;
	}
        sprintf(s, "%02x : ", i);
      }
      sprintf(a, "%02x ", packet[i]);
      strcat(s, a);
    }
    if (s[0]) {
      ESP_LOGI("pcp", "receive %s", s);
    }
  }
#endif
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void PcpClient::PcpReplyMapping(struct PcpPacket *rp) {
  bool found = false;
  ESP_LOGI("pcp", "ReplyMapping received, decoding ...");

  // Check nonce
  list<PcpMappingInventory>::iterator i;
  for (i = requests.begin(); i != requests.end(); i++) {
    if (i->nonce.isEqual(rp->mof.nonce)) {
      if (i->result_code == 0xFF && rp->rh.result_code == PCP_RESULT_SUCCESS) {
	ESP_LOGI("pcp", "Found matching nonce");
	found = true;

	// Register result code and timestamp
	i->result_code = PCP_RESULT_SUCCESS;
	struct timeval tv;
	gettimeofday(&tv, 0);
	i->timestamp = tv.tv_sec;

	// Copy external address
	for (int ix = 0; ix<4; ix++)
	  i->external_ip[ix] = rp->mof.external_ip[ix];

	ESP_LOGI("pcp", "Mapping succeeded : int %d ext %d, ext ip %s",
	  0xFFFF & ntohs(rp->mof.internal_port), 0xFFFF & ntohs(rp->mof.external_port),
	  inet_ntoa(rp->mof.external_ip[3]));
      }
    }
  }

  if (! found)
    ESP_LOGE("pcp", "PCP mapping failed : %s", resultCode2String(rp->rh.result_code));
}

const char *PcpClient::resultCode2String(int rc) {
  switch (rc) {
  case PCP_RESULT_SUCCESS:			return "success";
  case PCP_RESULT_UNSUPP_VERSION:		return "Unsupported version";
  case PCP_RESULT_NOT_AUTHORIZED:		return "not authorized";
  case PCP_RESULT_MALFORMED_REQUEST:		return "malformed request";
  case PCP_RESULT_UNSUPP_OPCODE:		return "unsupported opcode";
  case PCP_RESULT_UNSUPP_OPTION:		return "unsupported option";
  case PCP_RESULT_MALFORMED_OPTION:		return "malformed option";
  case PCP_RESULT_NETWORK_FAILURE:		return "network failure";
  case PCP_RESULT_NO_RESOURCES:			return "no resouress";
  case PCP_RESULT_UNSUPP_PROTOCOL:		return "unsupported protocol";
  case PCP_RESULT_USER_EX_QUOTA:		return "USER_EX_QUOTA";
  case PCP_RESULT_CANNOT_PROVIDE_EXTERNAL:	return "CANNOT_PROVIDE_EXTERNAL";
  case PCP_RESULT_ADDRESS_MISMATCH:		return "address mismatch";
  case PCP_RESULT_EXCESSIVE_REMOTE_PEERS:	return "excessive remote peers";
  default:					return "?";
  }
}

#if 0
void PcpClient::catchDeletePort(uint16_t len, char *data) {
  // Nothing to do ?

  // Success, so switch ourselves off
}
#endif

void PcpNonce::initialize() {
  a = 0;
  if (pcp)
    a = pcp->local;
  struct timeval tv;
  gettimeofday(&tv, 0);
  b = tv.tv_sec;
  c = tv.tv_usec;
}

bool PcpNonce::isEqual(PcpNonce other) {
  return (a == other.a) && (b == other.b) && (c == other.c);
}

void PcpNonce::copy(PcpNonce other) {
  a = other.a;
  b = other.b;
  c = other.c;
}
