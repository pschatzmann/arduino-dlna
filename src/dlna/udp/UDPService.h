#pragma once

#include "dlna/IUDPService.h"
#include "assert.h"

namespace tiny_dlna {

/**
 * @brief Access to UDP functionality: sending and receiving of data
 *
 * This class is a template and requires you to specify the UDP implementation type.
 * For example, use UDPService<WiFiUDP> for WiFiUDP or UDPService<MyUDPType> for a custom UDP class.
 *
 * Usage:
 *   UDPService<WiFiUDP> udp; // Uses WiFiUDP
 *   UDPService<MyUDPType> udp2; // Uses custom UDP type
 *
 * Implements IUDPService for sending and receiving UDP packets, including
 * multicast support.
 *
 * @author Phil Schatzmann
 */

template <typename UDPType>
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
      int len = udp.readBytes(tmp, packetSize);
      result.data = tmp;
      DlnaLogger.log(DlnaLogLevel::Info, "(%s [%d])->: %s", result.peer.toString(),
                     packetSize, tmp);
    }
    return result;
  }

 protected:
  UDPType udp;
  IPAddressAndPort peer;
  bool is_multicast = false;
};

}  // namespace tiny_dlna