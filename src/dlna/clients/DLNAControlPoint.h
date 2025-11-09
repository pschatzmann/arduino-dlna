#pragma once

#include <functional>

#include "Client.h"
#include "DLNAControlPointRequestParser.h"
#include "basic/Url.h"
#include "dlna/DLNADeviceInfo.h"
#include "dlna/Schedule.h"
#include "dlna/Scheduler.h"
#include "dlna/clients/IControlPoint.h"
#include "dlna/clients/SubscriptionMgrControlPoint.h"
#include "dlna/devices/DLNADevice.h"
#include "dlna/xml/XMLDeviceParser.h"
#include "dlna/xml/XMLParser.h"
#include "dlna/xml/XMLParserPrint.h"
#include "dlna_config.h"
#include "http/Http.h"

namespace tiny_dlna {

class DLNAControlPoint;
DLNAControlPoint* selfDLNAControlPoint = nullptr;

/**
 * @brief Lightweight DLNA control point manager
 *
 * This class implements a compact, embeddable DLNA/UPnP control point
 * suitable for small devices and emulators. It provides the core
 * responsibilities a control point needs in order to discover devices,
 * subscribe to service events, and invoke actions on services:
 *
 * - Discover devices using SSDP M-SEARCH and collect device/service
 *   descriptions (HTTP GET + XML parsing).
 * - Maintain a list of discovered `DLNADevice` objects and their
 *   `DLNAServiceInfo` entries.
 * - Subscribe to service event notifications (SUBSCRIBE) and accept
 *   incoming NOTIFY requests via an attached `HttpServer`.
 * - Parse event property-change XML and dispatch callbacks to the
 *   application (via `onNotification`).
 * - Execute SOAP actions on services (builds and posts SOAP XML).
 *
 * Notes and usage:
 * - To receive HTTP-based event notifications register an `HttpServer`
 *   with `setHttpServer()` (or construct with a server). The server's
 *   request handler will forward NOTIFY bodies to this manager.
 * - Register an application callback with `onNotification()` to receive
 *   parsed property updates. An opaque reference pointer may be stored
 *   with `setReference()` and will be passed back with the callback.
 * - Call `begin()` to start discovery and `loop()` frequently (e.g.
 *   from the device main loop) to process incoming network events and
 *   scheduled tasks.
 *
 * The class is intentionally small and avoids dynamic STL-heavy
 * constructs so it can run in constrained environments. It is not a
 * full-featured UPnP stack but provides the common features required by
 * many DLNA control point use cases.
 *
 * @author Phil Schatzmann
 */
class DLNAControlPoint : public IControlPoint {
 public:
  /// Default constructor w/o Notifications
  DLNAControlPoint() { selfDLNAControlPoint = this; }

  /// Constructor wiring HTTP and UDP dependencies; notifications disabled
  DLNAControlPoint(IHttpRequest& http, IUDPService& udp) : DLNAControlPoint() {
    setTransports(http, udp);
  }

  /// Constructor wiring HTTP and UDP dependencies; notifications disabled
  DLNAControlPoint(IHttpRequest& http, IUDPService& udp, IHttpServer& server)
      : DLNAControlPoint() {
    setTransports(http, udp);
    setHttpServer(server);
  }

  /// Constructor supporting Notifications
  DLNAControlPoint(IHttpServer& server) : DLNAControlPoint() {
    setHttpServer(server);
  }

  ~DLNAControlPoint() override = default;

  /// Requests the parsing of the device information
  void setParseDevice(bool flag) override { is_parse_device = flag; }

  /// Defines the local url (needed for subscriptions)
  void setLocalURL(Url url) override { local_url = url; }
  void setLocalURL(IPAddress url, int port=9001, const char* path="") override {
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "http://%s:%d%s", url.toString().c_str(), port,
             path);
    local_url.setUrl(buffer);
  }

  /// Sets the repeat interval for M-SEARCH requests (define before begin)
  void setSearchRepeatMs(int repeatMs) override {
    msearch_repeat_ms = repeatMs;
  }

  /// Attach an opaque reference pointer (optional, for caller context)
  void setReference(void* ref) override { reference = ref; }

  /// Selects the default device by index
  void setDeviceIndex(int idx) override { default_device_idx = 0; }

  /// Register a callback that will be invoked for incoming event notification
  void setEventSubscriptionCallback(
      std::function<void(const char* sid, const char* varName,
                         const char* newValue, void* reference)>
          cb,
      void* ref = nullptr) override {
    subscription_mgr.setEventSubscriptionCallback(cb, ref);
  }

  /// Set HttpServer instance and register the notify handler
  void setHttpServer(IHttpServer& server) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::setHttpServer");
    p_http_server = &server;
    subscription_mgr.setHttpServer(server);
  }

  /// Register a callback that will be invoked when parsing SOAP/Action results
  /// Signature: void(const char* nodeName, const char* text, const char*
  /// attributes)
  void onResultNode(std::function<void(const char* nodeName, const char* text,
                                       const char* attributes)>
                        cb) override {
    result_callback = cb;
  }

  bool begin(const char* searchTarget = "ssdp:all", uint32_t minWaitMs = 3000,
             uint32_t maxWaitMs = 60000) override {
    if (!p_http || !p_udp) {
      DlnaLogger.log(DlnaLogLevel::Error,
                     "DLNAControlPoint::begin: transports not configured");
      return false;
    }
    return begin(*p_http, *p_udp, searchTarget, minWaitMs, maxWaitMs);
  }

  /**
   * @brief Start discovery by sending M-SEARCH requests and process replies.
   *
   * searchTarget: the SSDP search target (e.g. "ssdp:all", device/service urn)
   * minWaitMs: minimum time in milliseconds to wait before returning a result
   *             (default: 3000 ms)
   * maxWaitMs: maximum time in milliseconds to wait for replies (M-SEARCH
   * window) (default: 60000 ms) Behavior: the function will wait at least
   * `minWaitMs` and at most `maxWaitMs`. If a device is discovered after
   * `minWaitMs` elapsed the function will return early; otherwise it will block
   * until `maxWaitMs`.
   */
  bool begin(IHttpRequest& http, IUDPService& udp,
             const char* searchTarget = "ssdp:all", uint32_t minWaitMs = 3000,
             uint32_t maxWaitMs = 60000) override {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::begin");
    http.setTimeout(DLNA_HTTP_REQUEST_TIMEOUT_MS);
    search_target = searchTarget;
    is_active = true;
    setTransports(http, udp);

    if (p_http_server) {
      // handle server requests
      if (!p_http_server->begin()) {
        DlnaLogger.log(DlnaLogLevel::Error, "HttpServer begin failed");
        return false;
      }
    }

    // setup multicast UDP
    if (!(p_udp->begin(DLNABroadcastAddress))) {
      DlnaLogger.log(DlnaLogLevel::Error, "UDP begin failed");
      return false;
    }

    // Send MSearch request via UDP. Use maxWaitMs as the emission window.
    MSearchSchedule* search =
        new MSearchSchedule(DLNABroadcastAddress, searchTarget);

    // ensure min <= max
    if (minWaitMs > maxWaitMs) minWaitMs = maxWaitMs;
    search->end_time = millis() + maxWaitMs;
    search->repeat_ms = msearch_repeat_ms;
    search->active = true;
    scheduler.add(search);

    // if maxWaitMs > 0 we will block here and process events. We guarantee
    // we wait at least minWaitMs before returning; we stop waiting after
    // maxWaitMs or earlier if stopWhenFound is true AND minWaitMs has elapsed
    uint64_t start = millis();
    uint64_t minEnd = start + minWaitMs;
    uint64_t maxEnd = start + maxWaitMs;
    while (millis() < maxEnd) {
      // if a device is found and we've satisfied the minimum wait, we can
      // return early (we always allow early return after minWaitMs when a
      // device has been discovered)
      if (devices.size() > 0 && millis() >= minEnd) break;
      loop();
    }

    // instantiate subscription manager
    subscription_mgr.setup(http, udp, local_url, getDevice());

    // If we exited early because a device was found, deactivate the MSearch
    // schedule so it will stop repeating. The scheduler will clean up
    // inactive schedules on its next pass.
    if (devices.size() > 0 && search != nullptr) {
      search->active = false;
    }

    DlnaLogger.log(DlnaLogLevel::Info,
                   "Control Point started with %d devices found",
                   devices.size());

    return devices.size() > 0;
  }

  void setTransports(IHttpRequest& http, IUDPService& udp) {
    p_http = &http;
    p_udp = &udp;
  }

  /// Stops the processing and releases the resources
  void end() override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::end");
    // stop active processing
    is_active = false;

    // drain scheduler (delete schedules). Execute will call cleanup which
    // deletes inactive schedules — run it until queue is empty. Only run if
    // we have a udp instance to avoid calling execute with a null reference.
    scheduler.setActive(false);
    if (p_udp) {
      while (scheduler.size() > 0) {
        scheduler.execute(*p_udp);
      }
    }
    // p_server->end();
    // clear device contents and drop containers
    for (auto& device : devices) device.clear();
    devices.clear();

    // clear pending actions
    actions.clear();

    // stop http server if attached
    if (p_http_server) {
      p_http_server->end();
      p_http_server = nullptr;
    }

    // reset xml printer state
    xml_printer.clear();

    // clear last reply
    reply.clear();
    local_url.clear();

    // reset transport/client refs
    p_http = nullptr;
    p_udp = nullptr;

    // reset indices and references
    default_device_idx = 0;
  }

  /// Registers a method that will be called
  ActionRequest& addAction(ActionRequest act) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::addAction");
    actions.push_back(act);
    return actions[actions.size() - 1];
  }

  /**
   * @brief Executes action and parses the reply xml to collect the reply
   * entries. If an XML processor is provided it will be used to process the
   * reply XML instead of the standard processing: This can be useful for large
   * replies where we do not want to store the big data chunks.
   * @param xmlProcessor Optional XML processor callback
   *
   */
  ActionReply& executeActions(XMLCallback xmlProcessor = nullptr) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::executeActions");
    reply.clear();
    postAllActions(xmlProcessor);

    // Default behaviour: log and dump collected reply arguments
    DlnaLogger.log(DlnaLogLevel::Info, "Collected reply arguments: %d",
                   (int)reply.size());
    reply.logArguments();
    return reply;
  }

  /// call this method in the Arduino loop as often as possible: the processes
  /// all replys
  bool loop() override {
    if (!is_active) return false;
    DLNAControlPointRequestParser parser;

    // process subscriptions
    subscription_mgr.loop();

    // process UDP requests
    RequestData req = p_udp->receive();
    if (req && scheduler.isMSearchActive()) {
      Schedule* schedule = parser.parse(req);
      if (schedule != nullptr) {
        // handle NotifyReplyCP
        if (StrView(schedule->name()).equals("NotifyReplyCP")) {
          NotifyReplyCP& notify_schedule = *(NotifyReplyCP*)schedule;
          // notify_schedule.callback = processDevice;
          processDevice(notify_schedule);
        }
        // handle MSearchReplyCP (HTTP/1.1 200 OK responses to M-SEARCH)
        if (StrView(schedule->name()).equals("MSearchReplyCP")) {
          MSearchReplyCP& ms_schedule = *(MSearchReplyCP*)schedule;
          // Process M-SEARCH replies immediately: they contain LOCATION URLs
          // pointing to device descriptions. Add the device and free the
          // temporary schedule object.
          processMSearchReply(ms_schedule);
        }
      }
    }

    // execute scheduled udp replys
    scheduler.execute(*p_udp);

    if (p_http_server && p_http_server->isActive()) {
      // handle server requests
      bool rc = p_http_server->doLoop();
    }

    // be nice, if we have other tasks
    delay(5);
    return true;
  }

  /// Provide addess to the service information
  DLNAServiceInfo& getService(const char* id) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getService");
    static DLNAServiceInfo no_service(false);
    for (auto& dev : devices) {
      DLNAServiceInfo& result = dev.getService(id);
      if (result) return result;
    }
    return no_service;
  }

  /// Provides the device information of the actually selected device
  DLNADeviceInfo& getDevice() override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getDevice");
    return devices[default_device_idx];
  }

  /// Provides the device information by index
  DLNADeviceInfo& getDevice(int idx) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getDevice");
    if (idx < 0 || idx >= devices.size()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Device index %d out of range", idx);
      return NO_DEVICE;
    }
    return devices[idx];
  }

  /// Provides the device for a service
  DLNADeviceInfo& getDevice(DLNAServiceInfo& service) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getDevice");
    for (auto& dev : devices) {
      for (auto& srv : dev.getServices()) {
        if (srv == service) return dev;
      }
    }
    return NO_DEVICE;
  }

  /// Get a device for a Url
  DLNADeviceInfo& getDevice(Url location) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getDevice");
    for (auto& dev : devices) {
      if (dev.getDeviceURL() == location) {
        DlnaLogger.log(DlnaLogLevel::Debug,
                       "DLNAControlPointMgr::getDevice: Found device %s",
                       location.url());
        return dev;
      }
    }
    return NO_DEVICE;
  }

  Vector<DLNADeviceInfo>& getDevices() override { return devices; }

  /**
   * Public wrapper to build a fully-qualified URL for a device + suffix.
   * This calls the protected getUrl() helper so external helpers can reuse
   * the control point's URL normalization logic without exposing the
   * internal protected method directly.
   */
  const char* getUrl(DLNADeviceInfo& device, const char* suffix, char* buffer,
                     int len) override {
    return getUrlImpl(device, suffix, buffer, len);
  }

  /// Adds a new device
  bool addDevice(DLNADeviceInfo dev) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::addDevice");
    for (auto& existing_device : devices) {
      if (dev.getUDN() == existing_device.getUDN()) {
        DlnaLogger.log(DlnaLogLevel::Debug, "Device '%s' already exists",
                       dev.getUDN());
        return false;
      }
    }
    DlnaLogger.log(DlnaLogLevel::Info, "Device '%s' has been added",
                   dev.getUDN());
    devices.push_back(dev);
    return true;
  }

  /// Adds the device from the device xml url if it does not already exist
  bool addDevice(Url url) override {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::addDevice");
    DLNADeviceInfo& device = getDevice(url);
    if (device != NO_DEVICE) {
      // device already exists
      device.setActive(true);
      return true;
    }
    // http get - incremental parse using XMLParserPrint so we don't hold
    // the whole device.xml in memory
    int rc = p_http->get(url, "text/xml");

    if (rc != 200) {
      DlnaLogger.log(DlnaLogLevel::Error, "Http get to '%s' failed with %d",
                     url.url(), rc);
      return false;
    }

    DLNADeviceInfo new_device;
    new_device.base_url.clear();
    new_device.device_url = url;

    XMLDeviceParser device_parser{new_device};
    uint8_t buffer[XML_PARSER_BUFFER_SIZE];

    // Read and incrementally parse into new_device
    device_parser.begin();
    while (true) {
      int len = p_http->read(buffer, sizeof(buffer));
      if (len <= 0) break;
      device_parser.parse(buffer, len);
    }
    device_parser.end(new_device);

    // if base_url is empty, derive it from the device description URL root
    if (new_device.base_url.isEmpty()) {
      new_device.base_url = url.urlRoot();
    }

    // Avoid adding the same device multiple times: check UDN uniqueness
    for (auto& existing_device : devices) {
      if (existing_device.getUDN() == new_device.getUDN()) {
        DlnaLogger.log(DlnaLogLevel::Debug,
                       "Device '%s' already exists (skipping add)",
                       new_device.getUDN());
        // Make sure the existing device is marked active and keep the URL
        existing_device.setActive(true);
        return true;
      }
    }

    DlnaLogger.log(DlnaLogLevel::Info, "Device '%s' has been added",
                   new_device.getUDN());
    devices.push_back(new_device);
    return true;
  }

  /// We can activate/deactivate the scheduler
  void setActive(bool flag) override { is_active = flag; }

  /// Checks if the scheduler is active
  bool isActive() override { return is_active; }

  /// Defines if localhost devices should be allowed
  void setAllowLocalhost(bool flag) override { allow_localhost = flag; }

  /// Provides the last reply
  ActionReply& getLastReply() override { return reply; }

  /// Provides the subscription manager
  SubscriptionMgrControlPoint* getSubscriptionMgr() override {
    return &subscription_mgr;
  }

  /// Activate/deactivate subscription notifications
  void setSubscribeNotificationsActive(bool flag) override {
    subscription_mgr.setEventSubscriptionActive(flag);
  }

  /// Register a string in the shared registry and return the stored pointer
  const char* registerString(const char* s) override {
    return s;  // registry removed
  }

 protected:
  DLNADeviceInfo NO_DEVICE{false};
  Scheduler scheduler;
  IHttpRequest* p_http = nullptr;
  IUDPService* p_udp = nullptr;
  SubscriptionMgrControlPoint subscription_mgr;
  Vector<DLNADeviceInfo> devices;
  Vector<ActionRequest> actions;
  ActionReply reply;
  XMLPrinter xml_printer;
  int default_device_idx = 0;
  int msearch_repeat_ms = 10000;
  bool is_active = false;
  bool is_parse_device = false;
  const char* search_target;
  Url local_url;
  bool allow_localhost = false;
  IHttpServer* p_http_server = nullptr;
  void* reference = nullptr;
  std::function<void(const char* nodeName, const char* text,
                     const char* attributes)>
      result_callback;

  /// Processes a NotifyReplyCP message
  /// Note: split into smaller helpers for clarity and testability.
  static bool handleNotifyByebye(Str& usn) {
    // delegate to existing bye handling
    return selfDLNAControlPoint->processBye(usn);
  }

  /// Returns true and sets outDev if a device with the UDN part of `usn_c` is
  /// already present in the device list.
  static bool isUdnKnown(const char* usn_c, DLNADeviceInfo*& outDev) {
    outDev = nullptr;
    if (!usn_c || *usn_c == '\0') return false;
    const char* sep = strstr(usn_c, "::");
    int udn_len = sep ? (int)(sep - usn_c) : (int)strlen(usn_c);
    for (auto& dev : selfDLNAControlPoint->devices) {
      const char* known_udn = dev.getUDN();
      if (known_udn && strncmp(known_udn, usn_c, udn_len) == 0 &&
          (int)strlen(known_udn) == udn_len) {
        outDev = &dev;
        return true;
      }
    }
    return false;
  }

  static bool handleNotifyAlive(NotifyReplyCP& data) {
    bool select = selfDLNAControlPoint->matches(data.usn.c_str());
    DlnaLogger.log(DlnaLogLevel::Debug, "addDevice: %s -> %s", data.usn.c_str(),
                   select ? "added" : "filtered");
    if (!select) return false;

    DLNADeviceInfo* existing = nullptr;
    if (isUdnKnown(data.usn.c_str(), existing)) {
      DlnaLogger.log(DlnaLogLevel::Debug,
                     "Device '%s' already known (skip GET)",
                     existing ? existing->getUDN() : "<unknown>");
      if (existing) existing->setActive(true);
      return true;
    }

    // Apply discovery netmask filtering before adding the device
    Url url{data.location.c_str()};
    if (!selfDLNAControlPoint->isDiscoveryAllowed(url)) {
      DlnaLogger.log(DlnaLogLevel::Info,
                     "Device '%s' filtered by netmask (LOCATION %s)",
                     data.usn.c_str(), url.host());
      return false;
    }

    // Not known and subnet accepted -> fetch and add device description
    selfDLNAControlPoint->addDevice(url);
    return true;
  }

  // Returns true if the LOCATION URL's host is in the same subnet as the
  // local_url host according to DLNA_DISCOVERY_NETMASK. If either side is
  // not a numeric IP literal or information is missing, the function
  // permits discovery (returns true).
  bool isDiscoveryAllowed(Url& url) {
    IPAddress netmask = DLNA_DISCOVERY_NETMASK;

    const char* peerHost = url.host();
    const char* localHost = local_url.host();

    IPAddress peerIP;
    IPAddress localIP;
    bool peerOK = (peerHost && *peerHost) ? peerIP.fromString(peerHost) : false;
    bool localOK =
        (localHost && *localHost) ? localIP.fromString(localHost) : false;

    // If we cannot parse both addresses, allow by default.
    if (!peerOK || !localOK) return true;

    for (int i = 0; i < 4; i++) {
      if ((localIP[i] & netmask[i]) != (peerIP[i] & netmask[i])) {
        DlnaLogger.log(DlnaLogLevel::Info,
                       "Discovery filtered: local=%d.%d.%d.%d peer=%d.%d.%d.%d "
                       "mask=%d.%d.%d.%d",
                       localIP[0], localIP[1], localIP[2], localIP[3],
                       peerIP[0], peerIP[1], peerIP[2], peerIP[3], netmask[0],
                       netmask[1], netmask[2], netmask[3]);
        return false;
      }
    }

    DlnaLogger.log(DlnaLogLevel::Debug,
                   "Discovery accepted: local=%d.%d.%d.%d peer=%d.%d.%d.%d",
                   localIP[0], localIP[1], localIP[2], localIP[3], peerIP[0],
                   peerIP[1], peerIP[2], peerIP[3]);
    return true;
  }

  static bool processDevice(NotifyReplyCP& data) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::processDevice");
    Str& nts = data.nts;
    if (nts.equals("ssdp:byebye")) {
      return handleNotifyByebye(nts);
    }
    if (nts.equals("ssdp:alive")) {
      return handleNotifyAlive(data);
    }
    return false;
  }

  /// Processes an M-SEARCH HTTP 200 reply and attempts to add the device
  static bool processMSearchReply(MSearchReplyCP& data) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "DLNAControlPointMgr::processMSearchReply");
    // data.location contains the device description URL
    if (data.location.isEmpty()) return false;

    const char* usn_c = data.usn.c_str();
    if (usn_c && *usn_c) {
      const char* sep = strstr(usn_c, "::");
      int udn_len = sep ? (int)(sep - usn_c) : (int)strlen(usn_c);
      for (auto& dev : selfDLNAControlPoint->devices) {
        const char* known_udn = dev.getUDN();
        if (known_udn && strncmp(known_udn, usn_c, udn_len) == 0 &&
            (int)strlen(known_udn) == udn_len) {
          DlnaLogger.log(DlnaLogLevel::Debug,
                         "MSearchReply: device '%s' already known (skip GET)",
                         known_udn);
          dev.setActive(true);
          return true;
        }
      }
    }

    Url url{data.location.c_str()};
    selfDLNAControlPoint->addDevice(url);
    return true;
  }

  /// checks if the usn contains the search target
  bool matches(const char* usn) {
    if (StrView(search_target).equals("ssdp:all")) return true;
    return StrView(usn).contains(search_target);
  }

  /// processes a bye-bye message
  bool processBye(Str& usn) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::processBye");
    for (auto& dev : devices) {
      if (usn.startsWith(dev.getUDN())) {
        for (auto& srv : dev.getServices()) {
          srv.is_active = false;
          if (usn.endsWith(srv.service_type)) {
            if (srv.is_active) {
              DlnaLogger.log(DlnaLogLevel::Info, "removeDevice: %s", usn);
              srv.is_active = false;
            }
          }
        }
      }
    }
    return false;
  }

  /**
   * Creates the Action Soap XML request. E.g
          "<?xml version=\"1.0\"?>\r\n"
          "<s:Envelope
     xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\r\n"
          "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
          "<s:Body>\r\n"
          "<u:SetTarget "
          "xmlns:u=\"urn:schemas-upnp-org:service:SwitchPower:1\">\r\n"
          "<newTargetValue>1</newTargetValue>\r\n"
          "</u:SetTarget>\r\n"
          "</s:Body>\r\n"
          "</s:Envelope>\r\n";

  */
  size_t createXML(ActionRequest& action) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::createXML");
    size_t result = xml_printer.printXMLHeader();

    result += xml_printer.printNodeBegin(
        "Envelope",
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"",
        "s");
    result += xml_printer.printNodeBegin("Body", nullptr, "s");

    char ns[200];
    StrView namespace_str(ns, 200);
    namespace_str = "xmlns:u=\"%1\"";
    bool ok = namespace_str.replace("%1", action.getService()->service_type);
    DlnaLogger.log(DlnaLogLevel::Debug, "ns = '%s'", namespace_str.c_str());

    // assert(ok);
    result += xml_printer.printNodeBegin(action.getAction(),
                                         namespace_str.c_str(), "u");
    for (auto arg : action.getArguments()) {
      const char* n = arg.name.c_str();
      const char* v = arg.value.c_str();
      if (n && *n) {
        result += xml_printer.printNode(n, v);
      }
    }
    result += xml_printer.printNodeEnd(action.getAction(), "u");

    result += xml_printer.printNodeEnd("Body", "s");
    result += xml_printer.printNodeEnd("Envelope", "s");
    return result;
  }

  ActionReply& postAllActions(XMLCallback xmlProcessor = nullptr) {
    reply.clear();
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::postAllActions");
    for (auto& action : actions) {
      if (action) postAction(action, xmlProcessor);
    }
    return reply;
  }

  ActionReply& postAction(ActionRequest& action,
                          XMLCallback xmlProcessor = nullptr) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::postAction: %s",
                   action.getAction());
    reply.clear();
    DLNAServiceInfo& service = *(action.getService());
    DLNADeviceInfo& device = getDevice(service);

    // create SOAPACTION header value
    char act[200];
    StrView action_str{act, 200};
    action_str = "\"";
    action_str.add(action.getService()->service_type);
    action_str.add("#");
    action_str.add(action.getAction());
    action_str.add("\"");

    // crate control url
    char url_buffer[DLNA_MAX_URL_LEN] = {0};
    // Log service and base to help debug malformed control URLs
    DlnaLogger.log(DlnaLogLevel::Info,
                   "Service control_url: %s, device base: %s",
                   StrView(service.control_url).c_str(),
                   StrView(device.getBaseURL()).c_str());
    Url post_url = getUrl(device, service.control_url, url_buffer, DLNA_MAX_URL_LEN);
    DlnaLogger.log(DlnaLogLevel::Info, "POST URL computed: %s", post_url.url());

    // send HTTP POST and collect/handle response. If the caller provided an
    // httpProcessor we forward it so they can process the raw HTTP reply
    // (and avoid the library's default XML parsing) before consumption.
    return processActionHttpPost(action, post_url, action_str.c_str(),
                                 xmlProcessor);
  }

  /// Build the SOAP XML request body for the action into `out`
  size_t createSoapXML(ActionRequest& action, Print& out) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::createSoapXML");
    xml_printer.setOutput(out);
    return createXML(action);
  }

  /// Processes an HTTP POST request for the given action and URL.
  ActionReply& processActionHttpPost(ActionRequest& action, Url& post_url,
                                     const char* soapAction,
                                     XMLCallback xmlProcessor = nullptr) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "DLNAControlPointMgr::processActionHttpPost");

    NullPrint np;
    size_t xmlLen = createSoapXML(action, np);
    // dynamically create and write xml
    auto printXML = [this, &action, &xmlLen](Print& out, void* ref) -> size_t {
      (void)ref;
      return createSoapXML(action, out);
    };
    p_http->stop();
    p_http->request().put("SOAPACTION", soapAction);
    int rc = p_http->post(post_url, xmlLen, printXML, "text/xml");

    // abort on HTTP error
    if (rc != 200) {
      p_http->stop();
      reply.setValid(false);
      DlnaLogger.log(DlnaLogLevel::Error, "Action '%s' failed with HTTP rc %d",
                     StrView(soapAction).c_str(), rc);
      return reply;
    }

    reply.setValid(true);
    reply.clear();

    // If caller provided a custom XML processor, let it handle the live
    // client/response. This allows callers to avoid the default XML parsing
    // which would otherwise consume the response stream.
    if (xmlProcessor) {
      Client* c = p_http->client();
      if (c) xmlProcessor(*c, reply);
      return reply;
    }

    // Default: parse response incrementally and populate reply arguments
    parseResult();
    return reply;
  }

  /**
   * @brief Parses the XML response from the HTTP client and populates reply
   * arguments.
   *
   * @param xml_parser XMLParserPrint instance for parsing
   * @param buffer Temporary buffer for reading client data
   */
  void parseResult() {
    XMLParserPrint xml_parser;
    xml_parser.setExpandEncoded(true);
    Str outNodeName;
    Vector<Str> outPath;
    Str outText;
    Str outAttributes;
    uint8_t buffer[XML_PARSER_BUFFER_SIZE];
    while (p_http->client()->available()) {
      int len = p_http->client()->read(buffer, XML_PARSER_BUFFER_SIZE);
      if (len > 0) {
        xml_parser.write(buffer, len);
        while (xml_parser.parse(outNodeName, outPath, outText, outAttributes)) {
          Argument arg;
          // For most nodes we only add arguments when they contain text or
          // attributes. The special "Result" node contains the DIDL-Lite
          // payload (raw XML) and must be preserved so callers (e.g. the
          // MediaServer helper) can parse returned media items.
          if (!(outText.isEmpty() && outAttributes.isEmpty()) ||
              outNodeName.equals("Result")) {
            unEscapeStr(outAttributes);
            unEscapeStr(outText);

            if (!outText.isEmpty()) {
              arg.name = outNodeName.c_str();
              arg.value = outText;
              reply.addArgument(arg);
            }

            DlnaLogger.log(DlnaLogLevel::Info, "Callback: '%s': %s (%s)",
                           StrView(outNodeName).c_str(),
                           StrView(outText).c_str(),
                           StrView(outAttributes).c_str());
            if (result_callback) {
              result_callback(
                  StrView(outNodeName ? outNodeName : "").c_str(),
                  StrView(outText ? outText : "").c_str(),
                  StrView(outAttributes ? outAttributes : "").c_str());
            }
          }
        }
      }
    }
    xml_parser.end();
  }

  void unEscapeStr(Str& str) {
    str.replaceAll("&quot;", "\"");
    str.replaceAll("&amp;", "&");
    str.replaceAll("&apos;", "'");
  }

  const char* getUrlImpl(DLNADeviceInfo& device, const char* suffix,
                         const char* buffer, int len) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getUrl");
    StrView url_str{(char*)buffer, len};
    // start with base url if available, otherwise construct from device URL
    const char* base = device.getBaseURL();
    if (base != nullptr && *base != '\0') {
      url_str = base;
    } else {
      url_str.add(device.getDeviceURL().protocol());
      url_str.add("://");
      url_str.add(device.getDeviceURL().host());
      url_str.add(":");
      url_str.add(device.getDeviceURL().port());
    }

    // Normalize slashes when appending suffix to avoid double '//'
    if (suffix != nullptr && suffix[0] != '\0') {
      int base_len = url_str.length();
      const char* tmpc = url_str.c_str();
      bool base_ends_slash =
          tmpc != nullptr && base_len > 0 && tmpc[base_len - 1] == '/';
      bool suf_starts_slash = suffix[0] == '/';
      if (base_ends_slash && suf_starts_slash) {
        // skip leading slash of suffix
        url_str.add(suffix + 1);
      } else if (!base_ends_slash && !suf_starts_slash) {
        // ensure single slash between base and suffix
        url_str.add('/');
        url_str.add(suffix);
      } else {
        // exactly one slash present
        url_str.add(suffix);
      }
    }
    return buffer;
  }
};

}  // namespace tiny_dlna