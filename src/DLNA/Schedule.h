#pragma once

#include "DLNADeviceInfo.h"
#include "UDPService.h"

#define MAX_TMP_SIZE 300
#define ALIVE_MS 5000

namespace tiny_dlna {

// multicast address for SSDP
static IPAddressAndPort DLNABroadcastAddress{IPAddress(239, 255, 255, 250),
                                             1900};
/**
 * @brief An individual Schedule (to send out UDP messages)
 * @author Phil Schatzmann
 */
struct Schedule {
  int id = 0;
  // scheduled next execution time
  uint32_t time = 0;
  // repeat every n ms
  int repeat_ms = 0;
  // repeat until;
  uint32_t end_time = 0;
  // schedle is active
  bool active = false;

  virtual bool process(UDPService &udp, DLNADeviceInfo &device) {
    return false;
  }

  virtual const char *name() { return "n/a"; };

  operator bool() { return active; }
};

/**
 * @brief Answer to MSearch request
 * @author Phil Schatzmann
 */
class MSearchReplySchedule : public Schedule {
 public:
  MSearchReplySchedule(IPAddressAndPort &addr) { p_addr = &addr; }
  const char *name() override { return "MSearchReply"; }

  bool process(UDPService &udp, DLNADeviceInfo &device) override {
    // we keep the data on the stack
    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: %s\r\n"
        "ST: %s\r\n"
        "USN: %s\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, max_age,
                     device.getDeviceURL().url(), device.getDeviceType(),
                     device.getUDN());
    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaDebug, "sending: %s", buffer);
    udp.send(*p_addr, (uint8_t *)buffer, n);
    return true;
  }

 protected:
  IPAddressAndPort *p_addr = nullptr;
  int max_age = 60;
};

/**
 * @brief Send out PostAlive messages: Repeated every 5 seconds
 * @author Phil Schatzmann
 */
class PostAliveSchedule : public Schedule {
 public:
  PostAliveSchedule(int repeatMs = ALIVE_MS) { this->repeat_ms = repeatMs; }
  const char *name() override { return "PostAlive"; }

  bool process(UDPService &udp, DLNADeviceInfo &device) override {
    // we keep the data on the stack
    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST:%s\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: *\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "USN: %s\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, DLNABroadcastAddress.toString(),
                     max_age, device.getDeviceType(), device.getUDN());

    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaDebug, "sending: %s", buffer);
    udp.send(DLNABroadcastAddress, (uint8_t *)buffer, n);
    return true;
  }

 protected:
  int max_age = 60;
};

/**
 * @brief Send out ByeBye message
 * @author Phil Schatzmann
 */
class PostByeSchedule : public Schedule {
 public:
  const char *name() override { return "ByeBye"; }
  bool process(UDPService &udp, DLNADeviceInfo &device) override {
    // we keep the data on the stack
    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: *\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:byebye\r\n"
        "USN: %s\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, DLNABroadcastAddress.toString(),
                     max_age, device.getDeviceType(), device.getUDN());

    DlnaLogger.log(DlnaDebug, "sending: %s", buffer);
    udp.send(DLNABroadcastAddress, (uint8_t *)buffer, n);
    return true;
  }

 protected:
  int max_age = 120;
};

}  // namespace tiny_dlna