#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
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

  bool begin(IPAddressAndPort addr) {
    peer = addr;
    DlnaLogger.log(DlnaInfo, "beginMulticast: %s", addr.toString());
    return udp.beginMulticast(addr.address, addr.port);
  }

  bool send(uint8_t *data, int len) { return send(peer, data, len); }

  bool send(IPAddressAndPort addr, uint8_t *data, int len) {
    DlnaLogger.log(DlnaDebug, "sending %d bytes", len);
    udp.beginPacket(addr.address, addr.port);
    int sent = udp.write(data, len);
    assert(sent == len);
    bool result = udp.endPacket();
    if (!result) {
      DlnaLogger.log(DlnaDebug, "Sending failed");
    }
    return result;
  }

  RequestData receive() {
    RequestData result;
    // udp.flush();
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      result.peer.address = udp.remoteIP();
      result.peer.port = udp.remotePort();
      char tmp[packetSize + 1] = {0};
      int len = udp.readBytes(tmp, len);
      result.data = tmp;
      DlnaLogger.log(DlnaInfo, "(%s [%d])->: %s", result.peer.toString(),
                     packetSize, tmp);
      Serial.write((uint8_t *)tmp, packetSize);
    }
    return result;
  }

 protected:
  WiFiUDP udp;
  IPAddressAndPort peer;
};

}  // namespace tiny_dlna