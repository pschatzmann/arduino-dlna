#pragma once

#include "DLNADevice.h"
#include "IUDPService.h"

#define MAX_TMP_SIZE 300
#define ALIVE_MS 0
#define MAX_AGE (60 * 60 * 24)

namespace tiny_dlna {


/**
 * @brief An individual Schedule (to send out UDP messages)
 * @author Phil Schatzmann
 */
struct Schedule {
  //int id = 0;
  // scheduled next execution time
  uint64_t time = 0;
  // repeat every n ms
  uint32_t repeat_ms = 0;
  // repeat until;
  uint64_t end_time = 0;
  // schedle is active
  bool active = false;

  virtual bool process(IUDPService &udp) { return false; }

  virtual const char *name() { return "n/a"; };

  operator bool() { return active; }
};

/**
 * @brief Send MSearch request
 * @author Phil Schatzmann
 */
class MSearchSchedule : public Schedule {
 public:
  MSearchSchedule(IPAddressAndPort addr, const char *searchTarget) {
    address = addr;
    search_target = searchTarget;
  }
  const char *name() override { return "MSearch"; }

  bool process(IUDPService &udp) override {
    // we keep the data on the stack
    DlnaLogger.log(DlnaLogLevel::Debug, "Sending %s for %s to %s", name(), search_target,
                   address.toString());

    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: %d\r\n"
        "ST: %s\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, address.toString(), max_age,
                     search_target);
    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaLogLevel::Info, "sending: %s", buffer);
    udp.send(address, (uint8_t *)buffer, n);
    return true;
  }

 protected:
  int max_age = 2;
  IPAddressAndPort address;
  const char *search_target = nullptr;
};

/**
 * @brief Answer from device to MSearch request by sending a reply
 * @author Phil Schatzmann
 */
class MSearchReplySchedule : public Schedule {
 public:
  MSearchReplySchedule(DLNADevice &device, IPAddressAndPort addr) {
    address = addr;
    p_device = &device;
  }
  const char *name() override { return "MSearchReply"; }

  bool process(IUDPService &udp) override {
    // we keep the data on the stack
    DlnaLogger.log(DlnaLogLevel::Info, "Sending %s for %s to %s", name(),
                   search_target.c_str(), address.toString());

    DLNADevice &device = *p_device;
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
    DlnaLogger.log(DlnaLogLevel::Debug, "sending: %s", buffer);
    udp.send(address, (uint8_t *)buffer, n);
    return true;
  }

  Str search_target;
  IPAddressAndPort address;
  DLNADevice *p_device;
  int mx = 0;

 protected:
  int max_age = MAX_AGE;
};

/**
 * @brief Processing at control point to handle a MSearchReply from
 * the device
 * @author Phil Schatzmann
 */
class MSearchReplyCP : public Schedule {
 public:
  const char *name() override { return "MSearchReplyCP"; }
  Str location;
  Str usn;
  Str search_target;

  bool process(IUDPService &udp) override {
    DlnaLogger.log(DlnaLogLevel::Info, "-> %s not processed", search_target.c_str());
    return true;
  }

};

class NotifyReplyCP : public MSearchReplyCP {
 public:
  const char *name() override { return "NotifyReplyCP"; }
  Str nts;
  Str delivery_host_and_port;
  Str delivery_path;
  Str subscription_id;
  Str event_key;
  Str xml;

  // callback 
  std::function<bool(NotifyReplyCP &ref)> callback;

  bool process(IUDPService &udp) override {
    if (callback(*this)){
      DlnaLogger.log(DlnaLogLevel::Info, "%s -> %s", name(), nts.c_str());
      return true;
    }

    DlnaLogger.log(DlnaLogLevel::Info, "-> %s not processed", nts.c_str());
    return true;
  }
};

/**
 * @brief Send out PostAlive messages: Repeated every 5 seconds
 * @author Phil Schatzmann
 */
class PostAliveSchedule : public Schedule {
 public:
  PostAliveSchedule(DLNADevice &device, uint32_t repeatMs) {
    p_device = &device;
    this->repeat_ms = repeatMs;
  }
  const char *name() override { return "PostAlive"; }

  void setRepeatMs(uint32_t ms) { this->repeat_ms = ms; }

  bool process(IUDPService &udp) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Sending %s to %s", name(),
                   DLNABroadcastAddress.toString());
    DLNADevice &device = *p_device;
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
  DLNADevice *p_device;
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
    DlnaLogger.log(DlnaLogLevel::Debug, "sending: %s", buffer);
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
  PostByeSchedule(DLNADevice &device) { p_device = &device; }
  const char *name() override { return "ByeBye"; }
  bool process(IUDPService &udp) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Sending %s to %s", name(),
                   DLNABroadcastAddress.toString());
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
                     max_age, p_device->getDeviceType(), p_device->getUDN());

    DlnaLogger.log(DlnaLogLevel::Debug, "sending: %s", buffer);
    udp.send(DLNABroadcastAddress, (uint8_t *)buffer, n);
    return true;
  }

 protected:
  int max_age = 120;
  DLNADevice *p_device;
};

/**
 * @brief Send SUBSCRIBE message via UDP unicast
 * @author Phil Schatzmann
 */
class PostSubscribe : public Schedule {
 public:
  PostSubscribe(IPAddressAndPort address, const char *path, uint32_t sec) {
    setDestination(address, path);
    setDuration(sec);
  }
  const char *name() override { return "Subscribe"; }

  bool process(IUDPService &udp) override {
    ///
    DlnaLogger.log(DlnaLogLevel::Info, "Sending Subscribe  to %s", address);

    char buffer[MAX_TMP_SIZE] = {0};
    const char *tmp =
        "SUBSCRIBE %s HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "CALLBACK: %s"
        "NT: upnp-event\r\n"
        "TIMEOUT: Second-%d\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, path, address.toString(),
                     durationSec);
    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaLogLevel::Debug, "sending: %s", buffer);
    udp.send(address, (uint8_t *)buffer, n);
    return true;
  }

 protected:
  IPAddressAndPort address;
  const char *path;
  int durationSec;

  void setDestination(IPAddressAndPort address, const char *path) {
    this->address = address;
    this->path = path;
  }

  void setDuration(uint32_t sec) { durationSec = sec; }
};

}  // namespace tiny_dlna