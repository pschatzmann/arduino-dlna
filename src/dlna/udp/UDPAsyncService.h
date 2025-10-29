#pragma once

#include <AsyncUDP.h>
#include <WiFi.h>

#include "assert.h"
#include "basic/QueueLockFree.h"
#include "dlna/IUDPService.h"

namespace tiny_dlna {

/**
 * @brief Access to UDP functionality: sending and receiving of data
 * using the Async API of the ESP32.
 * @author Phil Schatzmann
 */

class UDPAsyncService : public IUDPService {
 public:
  bool begin(int port) {
    IPAddressAndPort addr;
    addr.port = port;
    return begin(addr);
  }

  bool begin(IPAddressAndPort addr) {
    peer = addr;
    DlnaLogger.log(DlnaLogLevel::Info, "beginMulticast: %s", addr.toString());

    if (udp.listen(addr.port)) {
      udp.onPacket([&](AsyncUDPPacket packet) {
        RequestData result;
        result.peer.address = packet.remoteIP();
        result.peer.port = packet.remotePort();
        assert(!(result.peer.address == IPAddress()));
        // save data
        result.data.copyFrom((const char*)packet.data(), packet.length());

        // queue.push_back(result);
        queue.enqueue(result);
      });
    }

    if (udp.listenMulticast(addr.address,
                            addr.port)) {  // Start listening for UDP Multicast
                                           // on AP interface only
      udp.onPacket([&](AsyncUDPPacket packet) {
        RequestData result;
        result.peer.address = packet.remoteIP();
        result.peer.port = packet.remotePort();
        assert(!(result.peer.address == IPAddress()));
        // save data
        result.data.copyFrom((const char*)packet.data(), packet.length());

        // queue.push_back(result);
        queue.enqueue(result);
        DlnaLogger.log(DlnaLogLevel::Info, "UDPAsyncService::receive: received %d bytes", packet.length());

      });
    }
    return true;
  }

  bool send(uint8_t* data, int len) { return send(peer, data, len); }

  bool send(IPAddressAndPort addr, uint8_t* data, int len) {
    DlnaLogger.log(DlnaLogLevel::Debug, "sending %d bytes", len);
    int sent = udp.writeTo(data, len, addr.address, addr.port);
    if (sent != len) {
      DlnaLogger.log(DlnaLogLevel::Error, "sending %d bytes -> %d", len, sent);
    }
    return sent == len;
  }

  RequestData receive() {
    RequestData result;
    queue.dequeue(result);
    return result;
  }

 protected:
  AsyncUDP udp;
  IPAddressAndPort peer;
  //  Vector<RequestData> queue{50};
  QueueLockFree<RequestData> queue{50};
};

}  // namespace tiny_dlna