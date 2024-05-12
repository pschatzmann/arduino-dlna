#pragma once

#include "DLNADeviceInfo.h"
#include "IUDPService.h"

#define MAX_TMP_SIZE 300
#define ALIVE_MS 0

namespace tiny_dlna {

/**
 * @brief An individual Schedule (to send out UDP messages)
 * @author Phil Schatzmann
 */
struct Schedule {
  int id = 0;
  // scheduled next execution time
  uint32_t time = 0;
  // repeat every n ms
  uint32_t repeat_ms = 0;
  // repeat until;
  uint32_t end_time = 0;
  // schedle is active
  bool active = false;

  virtual bool process(IUDPService &udp, DLNADeviceInfo &device) {
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
  MSearchReplySchedule(IPAddressAndPort addr) { address = addr; }
  const char *name() override { return "MSearchReply"; }

  bool process(IUDPService &udp, DLNADeviceInfo &device) override {
    // we keep the data on the stack
    DlnaLogger.log(DlnaInfo, "Sending %s to %s", name(), address.toString());

    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: %s\r\n"
        "ST: %s\r\n"
        "USN: %s\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, max_age,
                     device.getDeviceURL().url(), search_target.c_str(),
                     device.getUDN());
    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaDebug, "sending: %s", buffer);
    udp.send(address, (uint8_t *)buffer, n);
    return true;
  }

  Str search_target;
  IPAddressAndPort address;
  int mx = 0;

 protected:
  int max_age = 60;
};

/**
 * @brief Send out PostAlive messages: Repeated every 5 seconds
 * @author Phil Schatzmann
 */
class PostAliveSchedule : public Schedule {
 public:
  PostAliveSchedule(uint32_t repeatMs = ALIVE_MS) { this->repeat_ms = repeatMs; }
  const char *name() override { return "PostAlive"; }

  void setRepeatMs(uint32_t ms){
    this->repeat_ms = ms;
  }

  bool process(IUDPService &udp, DLNADeviceInfo &device) override {
    DlnaLogger.log(DlnaInfo, "Sending %s to %s", name(), DLNABroadcastAddress.toString());
    char nt[100];
    char usn[200];
    const char *device_url = device.getDeviceURL().url();
    int max_age = 100;
    // announce with udn
    process(device.getUDN(), device.getUDN(), device_url, max_age, udp);
    // NT: upnp:rootdevice\r\n
    setupData("upnp:rootdevice", device.getUDN(), nt, usn);
    process(nt, usn, device_url, max_age, udp);
    // NT: urn:schemas-upnp-org:device:MediaRenderer:1\r\n
    setupData(device.getDeviceType(), device.getUDN(), nt, usn);
    process(nt, usn, device_url, max_age, udp);

    // announce with service udn
    for (auto &service : device.getServices()) {
      setupData(service.service_type, device.getUDN(), nt, usn);
      process(nt, usn, device_url, max_age, udp);
    }
    return true;
  }

 protected:
  void setupData(const char *nt, const char *udn, char *result_nt,
                 char *result_usn) {
    strcpy(result_nt, nt);
    strcpy(result_usn, udn);
    strcat(result_usn, "::");
    strcat(result_usn, nt);
  }

  bool process(const char *nt, const char *usn, const char *device_url,
               int max_age, IUDPService &udp) {
    // we keep the data on the stack
    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST:%s\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: %s\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "USN: %s\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, DLNABroadcastAddress.toString(),
                     max_age, device_url, nt, usn);

    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaDebug, "sending: %s", buffer);
    udp.send(DLNABroadcastAddress, (uint8_t *)buffer, n);
    return true;
  }
};

/**
 * @brief Send out ByeBye message
 * @author Phil Schatzmann
 */
class PostByeSchedule : public Schedule {
 public:
  const char *name() override { return "ByeBye"; }
  bool process(IUDPService &udp, DLNADeviceInfo &device) override {
    DlnaLogger.log(DlnaInfo, "Sending %s to %s", name(), DLNABroadcastAddress.toString());
    // we keep the data on the stack
    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: *\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:byebye\r\n"
        "USN: %s\r\n\r\n";
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