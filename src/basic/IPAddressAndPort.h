#pragma once
#include "IPAddress.h"
#include "assert.h"
#include "string.h"

namespace tiny_dlna {

static const char *toStr(IPAddress address) {
  static char result[20];
  sprintf(result, "%d.%d.%d.%d", address[0], address[1], address[2],
          address[3]);
  return result;
}

/**
 * @brief IP Adress including Port information
 * @author Phil Schatzmann
 */

struct IPAddressAndPort {
  IPAddressAndPort() = default;
  IPAddressAndPort(IPAddress addr, int prt) { address = addr, port = prt; }
  IPAddress address = IPAddress(0, 0, 0, 0);
  int port = 0;

  const char *toString() {
    static char result[80] = {0};
    int n = snprintf(result, 80, "%d.%d.%d.%d:%d", address[0], address[1],
                     address[2], address[3], port);
    assert(n < 80);
    return result;
  }
};

}  // namespace tiny_dlna