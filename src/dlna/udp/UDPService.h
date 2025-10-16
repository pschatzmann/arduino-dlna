#pragma once

#include <WiFi.h>
#include <WiFiUDP.h>
#include "dlna/IUDPService.h"
#include "assert.h"

namespace tiny_dlna {

/**
 * @brief Access to UDP functionality: sending and receiving of data
 * It seems that the UDP receive is not working for 
 * @author Phil Schatzmann
 */

class UDPService : public IUDPService {
 public:
  UDPService() = default;

  bool begin(int port) override {
    DlnaLogger.log(DlnaLogLevel::Info, "begin: %d", port);
    is_multicast = false;
    return udp.begin(port);
  }

  bool begin(IPAddressAndPort addr) override {
    peer = addr;
    is_multicast = true;
    DlnaLogger.log(DlnaLogLevel::Info, "beginMulticast: %s", addr.toString());
    return udp.beginMulticast(addr.address, addr.port);
  }

  bool send(uint8_t *data, int len) override { return send(peer, data, len); }

  bool send(IPAddressAndPort addr, uint8_t *data, int len) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "sending %d bytes", len);
    udp.beginPacket(addr.address, addr.port);
    int sent = udp.write(data, len);
    assert(sent == len);
    bool result = udp.endPacket();
    if (!result) {
      DlnaLogger.log(DlnaLogLevel::Error, "Sending failed");
    }
    return result;
  }

  RequestData receive() override {
    RequestData result;
    // udp.flush();
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      result.peer.address = udp.remoteIP();
      result.peer.port = udp.remotePort();
      char tmp[packetSize + 1] = {0};
      int len = udp.readBytes(tmp, len);
      result.data = tmp;
      DlnaLogger.log(DlnaLogLevel::Debug, "(%s [%d])->: %s", result.peer.toString(),
                     packetSize, tmp);
    }
    return result;
  }

 protected:
  WiFiUDP udp;
  IPAddressAndPort peer;
  bool is_multicast = false;
};

}  // namespace tiny_dlna