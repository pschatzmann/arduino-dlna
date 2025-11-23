#pragma once

#include "basic/IPAddressAndPort.h"
#include "dlna/common/DLNADeviceInfo.h"
#include "dlna/udp/IUDPService.h"

#define MAX_TMP_SIZE 400
#define ALIVE_MS 0
#define MAX_AGE (60 * 60 * 24)
#define MULTI_MSG_DELAY_MS 80

namespace tiny_dlna {

/**
 * @brief An individual Schedule (to send out UDP messages)
 * @author Phil Schatzmann
 */
struct Schedule {
  // int id = 0;
  //  scheduled next execution time
  uint64_t time = 0;
  // repeat every n ms
  uint32_t repeat_ms = 0;
  // repeat until;
  uint64_t end_time = 0;
  // schedle is active
  bool active = false;
  // optional address associated with the schedule (e.g. requester)
  IPAddressAndPort address;
  // enables logging the address when the schedule is queued
  bool report_ip = false;

  virtual bool process(IUDPService& udp) { return false; }

  virtual const char* name() { return "n/a"; };

  virtual bool isValid() { return true; }

  operator bool() { return active; }
};

/**
 * @brief Send MSearch request
 * @author Phil Schatzmann
 */
class MSearchSchedule : public Schedule {
 public:
  MSearchSchedule(IPAddressAndPort addr, const char* searchTarget, int mx = 3) {
    this->address = addr;
    search_target = searchTarget;
    max_age = mx;
  }
  const char* name() override { return "MSearch"; }

  bool process(IUDPService& udp) override {
    // we keep the data on the stack
    DlnaLogger.log(DlnaLogLevel::Debug, "Sending %s for %s to %s", name(),
                   search_target, address.toString());

    char buffer[MAX_TMP_SIZE] = {0};
    const char* tmp =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: %d\r\n"
        "ST: %s\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, address.toString(), max_age,
                     search_target);
    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaLogLevel::Debug, "sending: %s", buffer);
    udp.send(address, (uint8_t*)buffer, n);
    return true;
  }

 protected:
  int max_age = 3;
  const char* search_target = nullptr;
};

/**
 * @brief Answer from device to MSearch request by sending the related replies
 * @author Phil Schatzmann
 */
class MSearchReplySchedule : public Schedule {
 public:
  MSearchReplySchedule(DLNADeviceInfo& device, IPAddressAndPort addr) {
    this->address = addr;
    this->report_ip = true;
    p_device = &device;
  }
  const char* name() override { return "MSearchReply"; }

  bool process(IUDPService& udp) override {
    // we keep the data on the stack
    DlnaLogger.log(DlnaLogLevel::Info, "Sending %s for %s to %s", name(),
                   search_target.c_str(), address.toString());

    DLNADeviceInfo& device = *p_device;
    const char* udn = device.getUDN();
    int delay_ms = MULTI_MSG_DELAY_MS;

    sendReply(udp, device, "upnp:rootdevice", udn);
    delay(delay_ms);
    sendReply(udp, device, p_device->getDeviceType(), udn);
    // announce with service udn
    for (auto& service : device.getServices()) {
      delay(delay_ms);
      sendReply(udp, device, service.service_type, udn);
    }
    return true;
  }

  bool isValid() override {
    // check if the search target is valid for this device
    if (!isValidSearchTarget(*p_device, search_target.c_str())) {
      return false;
    }
    return isValidIP();
  }

  Str search_target;
  DLNADeviceInfo* p_device;
  int mx = 0;

 protected:
  int max_age = MAX_AGE;

  /// check netmask
  bool isValidIP() {
    IPAddress netmask = DLNA_DISCOVERY_NETMASK;
    IPAddress localIP = p_device->getIPAddress();
    IPAddress peerIP = address.address;
    Str netmask_str = netmask.toString().c_str();
    Str localIP_str = localIP.toString().c_str();

    // Apply netmask to both local and peer IP addresses
    for (int i = 0; i < 4; i++) {
      if ((localIP[i] & netmask[i]) != (peerIP[i] & netmask[i])) {
        DlnaLogger.log(DlnaLogLevel::Info,
                       "Discovery request from %s filtered (not in same subnet "
                       "as %s with mask %s)",
                       address.toString(), localIP_str.c_str(),
                       netmask_str.c_str());
        return false;
      }
    }
    return true;
  }

  // check requested search target
  bool isValidSearchTarget(DLNADeviceInfo& device, const char* search_target) {
    if (StrView(search_target).equals("ssdp:all")) {
      return true;
    }
    if (StrView(search_target).equals("upnp:rootdevice")) {
      return true;
    }
    if (StrView(search_target).equals(device.getDeviceType())) {
      return true;
    }
    // check services
    for (auto& service : device.getServices()) {
      if (StrView(search_target).equals(service.service_type)) {
        return true;
      }
    }
    DlnaLogger.log(DlnaLogLevel::Info, "Ignoring M-SEARCH for %s",
                   search_target);
    return false;
  }

  bool sendReply(IUDPService& udp, DLNADeviceInfo& device, const char* target,
                 const char* udn) {
    // construct usn
    char usn_str[200];
    snprintf(usn_str, 200, "%s::%s", device.getUDN(), target);
    // construct message
    char buffer[MAX_TMP_SIZE] = {0};
    const char* tmp =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=%d\r\n"
        "EXT:\r\n"
        "LOCATION: %s\r\n"
        "SERVER: Arduino-DLNA/1.0 UPnP/1.1 DLNA/1.5\r\n"
        "ST: %s\r\n"
        "USN: %s\r\n"
        "CONTENT-LENGTH: 0\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, max_age,
                     device.getDeviceURL().url(), target, usn_str);
    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaLogLevel::Info, "- %s: %s", name(), target);
    DlnaLogger.log(DlnaLogLevel::Debug, "%s", buffer);
    if (!udp.send(address, (uint8_t*)buffer, n)) {
      DlnaLogger.log(DlnaLogLevel::Warning, "Failed to send MSearchReply to %s",
                     address.toString());
      return false;
    }
    return true;
  }
};

/**
 * @brief Processing at control point to handle a MSearchReply from
 * the device
 * @author Phil Schatzmann
 */
class MSearchReplyCP : public Schedule {
 public:
  const char* name() override { return "MSearchReplyCP"; }
  Str location;
  Str usn;
  Str search_target;

  bool process(IUDPService& udp) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "-> %s not processed",
                   search_target.c_str());
    return true;
  }
};

/**
 * @class NotifyReplyCP
 * @brief Represents a notification/notify reply scheduled for control-point
 * processing.
 *
 * Instances are created when the control-point receives SSDP/HTTP notify
 * messages (for example via an attached HTTP server or UDP listener). The
 * object carries parsed headers and the event payload and provides a
 * `callback` that the control-point can invoke to let an application
 * consumer handle the notification.
 *
 * Fields of interest:
 * - `nts` : the NTS header value (e.g. "ssdp:alive" or "ssdp:byebye").
 * - `delivery_host_and_port` : host:port that delivered the notification.
 * - `delivery_path` : path portion of the delivery URL (if available).
 * - `subscription_id` : SID header (subscription identifier) for eventing.
 * - `event_key` : optional event sequence key.
 * - `xml` : raw event XML body (for property change parsing).
 * - `callback` : application-provided handler with signature
 *                `bool callback(NotifyReplyCP &ref)`. The handler should
 *                return true if it handled the notification, false if it
 *                did not.
 *
 * The `process()` implementation calls `callback` and logs whether the
 * notification was handled. This struct inherits from `MSearchReplyCP` so
 * it can be scheduled and executed by the control-point scheduler.
 */
class NotifyReplyCP : public MSearchReplyCP {
 public:
  const char* name() override { return "NotifyReplyCP"; }
  Str nts{80};
  Str delivery_host_and_port;
  Str delivery_path;
  Str subscription_id;
  Str event_key;
  Str xml;

  // callback invoked by the scheduler when processing this notification.
  // Return true if the notification was handled; false otherwise.
  std::function<bool(NotifyReplyCP& ref)> callback;

  bool process(IUDPService& udp) override {
    if (callback(*this)) {
      DlnaLogger.log(DlnaLogLevel::Debug, "%s -> %s", name(), nts.c_str());
      return true;
    }

    DlnaLogger.log(DlnaLogLevel::Debug, "-> %s not processed", nts.c_str());
    return true;
  }
};

/**
 * @brief Send out PostAlive messages: Repeated every 5 seconds
 * @author Phil Schatzmann
 */
class PostAliveSchedule : public Schedule {
 public:
  PostAliveSchedule(DLNADeviceInfo& device, uint32_t repeatMs) {
    p_device = &device;
    this->repeat_ms = repeatMs;
  }
  const char* name() override { return "PostAlive"; }

  void setRepeatMs(uint32_t ms) { this->repeat_ms = ms; }

  bool process(IUDPService& udp) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "Sending %s to %s", name(),
                   DLNABroadcastAddress.toString());
    DLNADeviceInfo& device = *p_device;
    char nt[100];

    const char* device_url = device.getDeviceURL().url();
    const char* udn = device.getUDN();
    int max_age = repeat_ms / 1000 + 10;
    int delay_ms = MULTI_MSG_DELAY_MS;

    // announce with udn
    sendData(udn, udn, device_url, max_age, udp);
    // NT: upnp:rootdevice\r\n
    sendData("upnp:rootdevice", udn, device_url, max_age, udp);
    delay(delay_ms);
    // NT: urn:schemas-upnp-org:device:MediaRenderer:1\r\n
    sendData(device.getDeviceType(), udn, device_url, max_age, udp);

    // announce with service udn
    for (auto& service : device.getServices()) {
      delay(delay_ms);
      sendData(service.service_type, udn, device_url, max_age, udp);
    }
    return true;
  }

 protected:
  DLNADeviceInfo* p_device;

  bool sendData(const char* nt, const char* udn, const char* device_url,
                int max_age, IUDPService& udp) {
    char usn[200];
    if (StrView(nt).equals(udn)) {
      strcpy(usn, nt);
    } else {
      snprintf(usn, 200, "%s::%s", udn, nt);
    }

    // we keep the data on the stack
    char buffer[MAX_TMP_SIZE] = {0};
    const char* tmp =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST:%s\r\n"
        "CACHE-CONTROL: max-age=%d\r\n"
        "LOCATION: %s\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "USN: %s\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, DLNABroadcastAddress.toString(),
                     max_age, device_url, nt, usn);

    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaLogLevel::Info, "sending: ssdp:alive %s", nt);
    DlnaLogger.log(DlnaLogLevel::Debug, "%s", buffer);
    udp.send(DLNABroadcastAddress, (uint8_t*)buffer, n);
    return true;
  }
};

/**
 * @brief Send out ByeBye message
 * @author Phil Schatzmann
 */
class PostByeSchedule : public Schedule {
 public:
  PostByeSchedule(DLNADeviceInfo& device) { p_device = &device; }
  const char* name() override { return "ByeBye"; }
  bool process(IUDPService& udp) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "Sending %s to %s", name(),
                   DLNABroadcastAddress.toString());
    // we keep the data on the stack
    char buffer[MAX_TMP_SIZE] = {0};
    const char* tmp =
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
    udp.send(DLNABroadcastAddress, (uint8_t*)buffer, n);
    return true;
  }

 protected:
  int max_age = 1800;
  DLNADeviceInfo* p_device;
};

/**
 * @brief Send SUBSCRIBE message via UDP unicast
 * @author Phil Schatzmann
 */
class PostSubscribe : public Schedule {
 public:
  PostSubscribe(IPAddressAndPort addr, const char* path, uint32_t sec) {
    setDestination(addr, path);
    setDuration(sec);
  }
  const char* name() override { return "Subscribe"; }

  bool process(IUDPService& udp) override {
    ///
    DlnaLogger.log(DlnaLogLevel::Debug, "Sending Subscribe  to %s",
                   address.toString());

    char buffer[MAX_TMP_SIZE] = {0};
    const char* tmp =
        "SUBSCRIBE %s HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "CALLBACK: %s"
        "NT: upnp-event\r\n"
        "TIMEOUT: Second-%d\r\n\r\n";
    int n = snprintf(buffer, MAX_TMP_SIZE, tmp, path, address.toString(),
                     durationSec);
    assert(n < MAX_TMP_SIZE);
    DlnaLogger.log(DlnaLogLevel::Debug, "sending: %s", buffer);
    udp.send(address, (uint8_t*)buffer, n);
    return true;
  }

 protected:
  const char* path;
  int durationSec = 0;

  void setDestination(IPAddressAndPort addr, const char* path) {
    this->address = addr;
    this->path = path;
  }

  void setDuration(uint32_t sec) { durationSec = sec; }
};

/**
 * @brief Generic Schedule that invokes a callback function
 * @author Phil Schatzmann
 */
class CallbackSchedule : public Schedule {
 public:
  CallbackSchedule(std::function<bool(void* ref)> cb, void* ref) {
    callback = cb;
    reference = ref;
  }
  const char* name() override { return "Callback"; }
  void* reference = nullptr;

  bool process(IUDPService& udp) override {
    if (callback) {
      return callback(reference);
    }
    return false;
  }

 protected:
  std::function<bool(void* ref)> callback;
};

}  // namespace tiny_dlna