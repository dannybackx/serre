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
 */
#ifndef _INCLUDE_PCPCLIENT_H_
#define _INCLUDE_PCPCLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <esp_event.h>

#include <list>
using namespace std;

class PcpNonce {
  public:
    void initialize();
    bool isEqual(PcpNonce other);
    void copy(PcpNonce other);

  private:
    uint32_t	a, b, c;
};
struct PcpMappingInventory;

class PcpClient {
public:
  PcpClient();
  ~PcpClient();

  void setLocalIP(in_addr_t);
  void setRouter(const char *);
  void setRouter(const in_addr_t);
  void queryRouterExternalAddress();
  in_addr_t getRouterExternalAddress();
  void addPort(int16_t localport, int16_t remoteport, int8_t protocol, int32_t lifetime);
  void deletePort(int16_t localport, int8_t protocol);

  esp_err_t NetworkConnected(void *ctx, system_event_t *event);
  const char *resultCode2String(int rc);

private:
  in_addr_t	local,
  		external,
		router_ip;
  char		*router_name;

  int		sock;
  friend void pcp_task(void *ptr);	// to access sock
  friend void PcpNonce::initialize();

  const int	pcp_client_port = 5350,
  		pcp_server_port = 5351;

  void sendPacket(const char *packet, const int len);
  TaskHandle_t task;
  void PcpReplyMapping(struct PcpPacket *);

  list<PcpMappingInventory> requests;
};

struct PcpMappingInventory {
  uint8_t	result_code;
  uint32_t	lifetime;
  PcpNonce	nonce;
  uint8_t	protocol;
  uint16_t	internal_port;
  uint16_t	external_port;
  uint32_t	external_ip[4];
};

struct PcpRequestHeader {
  uint8_t version;		// 2
  uint8_t opcode;		// 0 for request, 1 for reply
  uint8_t reserved;
  uint8_t result_code;		// Actually in request this is also reserved, mandatory 0
  uint32_t lifetime;		// in seconds
  uint32_t client_ip[4];
};

struct PcpMappingOpcodeFields {
  PcpNonce	nonce;		// 96 bits of nonce
  uint8_t	protocol;
  uint8_t	reserved[3];	// 24 bits
  uint16_t	internal_port;
  uint16_t	external_port;
  uint32_t	external_ip[4];	// 128 bits
};

struct PcpPeerOpcodeFields {
  PcpNonce	nonce;		// Mapping nonce, 96 bits
  uint8_t	protocol;	// Protocol
  uint8_t	reserved[3];	// Reserved, 24 bits
  uint16_t	internal_port;	// Internal Port
  uint16_t	external_port;	// Suggested External Port
  uint32_t	external_ip[4];	// Suggested External IP Address, 128 bits
  uint16_t	remote_port;	// Remote Peer Port, 16 bits
  uint16_t	reserved2;	// Reserved, 16 bits
  uint32_t	remote_ip[4];	// Remote Peer IP Address, 128 bits
};

struct PcpPacket {
  struct PcpRequestHeader	rh;
  struct PcpMappingOpcodeFields	mof;
};

enum PcpResultCode {
  PCP_RESULT_SUCCESS = 0,
  PCP_RESULT_UNSUPP_VERSION,
  PCP_RESULT_NOT_AUTHORIZED,
  PCP_RESULT_MALFORMED_REQUEST,
  PCP_RESULT_UNSUPP_OPCODE,
  PCP_RESULT_UNSUPP_OPTION,
  PCP_RESULT_MALFORMED_OPTION,
  PCP_RESULT_NETWORK_FAILURE,
  PCP_RESULT_NO_RESOURCES,
  PCP_RESULT_UNSUPP_PROTOCOL,
  PCP_RESULT_USER_EX_QUOTA,
  PCP_RESULT_CANNOT_PROVIDE_EXTERNAL,
  PCP_RESULT_ADDRESS_MISMATCH,
  PCP_RESULT_EXCESSIVE_REMOTE_PEERS
};

enum PcpOpcode {
  PCP_OPCODE_ANNOUNCE = 0,
  PCP_OPCODE_MAP = 1,
  PCP_OPCODE_PEER = 2
};

enum PcpMapOptionCode {
  PCP_MAP_OPTION_THIRD_PARTY = 1,
  PCP_MAP_OPTION_PREFER_FAILURE = 2,
  PCP_MAP_OPTION_FILTER = 3
};

extern PcpClient *pcp;

#endif	/* _INCLUDE_PCPCLIENT_H_ */
