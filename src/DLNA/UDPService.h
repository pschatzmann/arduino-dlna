#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include "../Basic/Str.h"
#include "assert.h"

namespace tiny_dlna {

/**
 * @brief IP Adress including Port information
 * @author Phil Schatzmann
 */

struct IPAddressAndPort {
  IPAddressAndPort() = default;
  IPAddressAndPort(IPAddress addr, int prt) { address = addr, port = prt; }
  IPAddress address;
  int port;

  const char *toString() {
    static char result[80] = {0};
    int n = snprintf(result, 80, "%d.%d.%d.%d:%d", address[0], address[1], address[2], address[3],
            port);
    assert(n<80);
    return result;
  }
};

/**
 * @brief Provides information of the received UDP which consists of the (xml)
 * data and the peer address and port
 * @author Phil Schatzmann
 */

struct RequestData {
  RequestData() = default;
  Str data;
  IPAddressAndPort peer;
  operator bool() { return !data.isEmpty(); }
};

/**
 * @brief Access to UDP functionality: sending and receiving of data
 * @author Phil Schatzmann
 */

class UDPService {
 public:
  UDPService(WiFiUDP &udp) { p_udp = &udp; }

  bool send(IPAddressAndPort addr, uint8_t *data, int len) {
    p_udp->beginPacket(addr.address, addr.port);
    int sent = p_udp->write(data, len);
    assert(sent == len);
    bool result = p_udp->endPacket();
    return result;
  }

  RequestData receive() {
    RequestData result;
    p_udp->flush();
    int packetSize = p_udp->parsePacket();
    if (packetSize) {
      result.peer.address = p_udp->remoteIP();
      result.peer.port = p_udp->remotePort();
      char tmp[packetSize + 1] = {0};
      int len = p_udp->read(tmp, len);
      result.data = tmp;
    }
    return result;
  }

 protected:
  WiFiUDP *p_udp = nullptr;
};

} // ns