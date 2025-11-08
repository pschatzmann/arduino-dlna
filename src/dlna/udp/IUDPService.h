#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

#include "dlna_config.h"
#include "basic/IPAddressAndPort.h"
#include "basic/Str.h"
#include "assert.h"

namespace tiny_dlna {

static IPAddressAndPort DLNABroadcastAddress{IPAddress(239, 255, 255, 250),
                                             DLNA_SSDP_PORT};

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
  virtual ~IUDPService() = default;
  /// Initialize UDP service on specified port
  virtual bool begin(int port) = 0;
  /// Initialize UDP service on specified address and port
  virtual bool begin(IPAddressAndPort addr) = 0;
  /// Send data to the default destination
  virtual bool send(uint8_t* data, int len) = 0;
  /// Send data to specified address and port
  virtual bool send(IPAddressAndPort addr, uint8_t* data, int len) = 0;
  /// Receive incoming UDP data and peer information
  virtual RequestData receive() = 0;
};

}  // namespace tiny_dlna
