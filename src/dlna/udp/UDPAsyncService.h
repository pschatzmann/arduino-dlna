#pragma once

#include <AsyncUDP.h>
#include <WiFi.h>

#include "assert.h"
#include "basic/Queue.h"
#include "concurrency/QueueLockFree.h"
#include "dlna/udp/IUDPService.h"

namespace tiny_dlna {

// forward-declare RequestData so we can use Queue<RequestData> in this header
struct RequestData;

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
        bool ok = queue.enqueue(result);
        if (!ok) {
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "UDPAsyncService::enqueue failed: queue full, packet "
                         "%d bytes, queueSize=%d",
                         packet.length(), (int)queue.size());
        }
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
        bool ok = queue.enqueue(result);
        if (!ok) {
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "UDPAsyncService::enqueue failed: multicast queue "
                         "full, packet %d bytes, queueSize=%d",
                         packet.length(), (int)queue.size());
        }
        DlnaLogger.log(DlnaLogLevel::Info,
                       "UDPAsyncService::receive: received %d bytes",
                       packet.length());
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
  // keep the lock-free variant available but commented out for now
  // QueueLockFree<RequestData> queue{50};
  // use simple FIFO queue (backed by List)
  Queue<RequestData> queue;
};

}  // namespace tiny_dlna