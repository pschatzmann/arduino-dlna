
#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

#include "basic/IPAddressAndPort.h"
#include "basic/Str.h"
#include "assert.h"

namespace tiny_dlna {

// multicast address for SSDP
static IPAddressAndPort DLNABroadcastAddress{IPAddress(239, 255, 255, 250),
                                             1900};

/**
 * @brief Provides information of the received UDP which consists of the (xml)
 * data and the peer address and port
 * @author Phil Schatzmann
 */

struct RequestData {
  Str data{0};
  IPAddressAndPort peer;
  operator bool() { return !data.isEmpty(); }
};

/**
 * @brief Abstract Interface for UDP API
 * @author Phil Schatzmann
*/

class IUDPService {
 public:
  virtual bool begin(IPAddressAndPort addr) = 0;
  virtual bool send(uint8_t *data, int len) = 0;
  virtual bool send(IPAddressAndPort addr, uint8_t *data, int len) = 0;
  virtual RequestData receive() = 0;
};

}  // namespace tiny_dlna
