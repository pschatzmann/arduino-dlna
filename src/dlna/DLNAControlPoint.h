#pragma once

#include <functional>

#include "DLNAControlPointRequestParser.h"
#include "DLNADevice.h"
#include "DLNADeviceInfo.h"
#include "Schedule.h"
#include "Scheduler.h"
#include "basic/StrPrint.h"
#include "basic/Url.h"
#include "dlna_config.h"
#include "http/Http.h"
#include "xml/XMLDeviceParser.h"
#include "xml/XMLParser.h"
#include "xml/XMLParserPrint.h"

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
class DLNAControlPoint {
 public:
  /// Default constructor w/o Notifications
  DLNAControlPoint() { selfDLNAControlPoint = this; }

  /// Constructor supporting Notifications
  DLNAControlPoint(HttpServer& server, int port = 80) : DLNAControlPoint() {
    setHttpServer(server, port);
  }
  /// Requests the parsing of the device information
  void setParseDevice(bool flag) { is_parse_device = flag; }

  /// Defines the local url (needed for subscriptions)
  void setLocalURL(Url url) { local_url = url; }

  /// Sets the repeat interval for M-SEARCH requests (define before begin)
  void setSearchRepeatMs(int repeatMs) { msearch_repeat_ms = repeatMs; }

  /// Attach an opaque reference pointer (optional, for caller context)
  void setReference(void* ref) { reference = ref; }

  /// Selects the default device by index
  void setDeviceIndex(int idx) { default_device_idx = 0; }

  /// Activates event notification subscription with repeat interval in seconds
  void setSubscribeInterval(int repeatSec = 3600) {
    subscribe_repeat_sec = repeatSec;
  }

  /// Register a callback that will be invoked for incoming event notification
  void onNotification(
      std::function<void(void* reference, const char* sid, const char* varName,
                         const char* newValue)>
          cb) {
    event_callback = cb;
  }

  /// Set HttpServer instance and register the notify handler
  void setHttpServer(HttpServer& server, int port = 80) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::setHttpServer");
    p_http_server = &server;
    http_server_port = port;
    attachHttpServer(server);
  }

  /// Register a callback that will be invoked when parsing SOAP/Action results
  /// Signature: void(const char* nodeName, const char* text, const char*
  /// attributes)
  void onResultNode(std::function<void(const char* nodeName, const char* text,
                                       const char* attributes)>
                        cb) {
    result_callback = cb;
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
  bool begin(DLNAHttpRequest& http, IUDPService& udp,
             const char* searchTarget = "ssdp:all", uint32_t minWaitMs = 3000,
             uint32_t maxWaitMs = 60000) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::begin");
    http.setTimeout(DLNA_HTTP_REQUEST_TIMEOUT_MS);
    search_target = searchTarget;
    is_active = true;
    p_udp = &udp;
    p_http = &http;

    // set timeout for http requests
    http.setTimeout(DLNA_HTTP_REQUEST_TIMEOUT_MS);

    if (p_http_server && http_server_port > 0 && event_callback != nullptr) {
      // handle server requests
      if (!p_http_server->begin(http_server_port)) {
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

  /// Subscribes to event notifications for the selected device
  bool subscribeNotifications() {
    // activate subscriptions
    if (event_callback != nullptr) {
      if (!subscribeNotifications(getDevice(), subscribe_repeat_sec)) {
        DlnaLogger.log(DlnaLogLevel::Error,
                       "Failed to subscribe to device notifications");
        return false;
      }
      return true;
    }
    return false;
  }

  /// Stops the processing and releases the resources
  void end() {
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

    // clear string registry (frees owned strings)
    registry.clear();

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
  ActionRequest& addAction(ActionRequest act) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::addAction");
    actions.push_back(act);
    return actions[actions.size() - 1];
  }

  /// Executes all registered methods
  ActionReply& executeActions() {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::executeActions");
    reply.clear();
    postAllActions();
    // Debug: dump collected reply arguments for verification
    DlnaLogger.log(DlnaLogLevel::Info, "Collected reply arguments: %d",
                   (int)reply.size());
    reply.logArguments();
    return reply;
  }

  /// call this method in the Arduino loop as often as possible: the processes
  /// all replys
  bool loop() {
    if (!is_active) return false;
    DLNAControlPointRequestParser parser;

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

      // re-subscribe after timeout
      if (event_callback != nullptr && subscribe_timeout > 0 &&
          millis() >= subscribe_timeout) {
        subscribeNotifications(getDevice(), subscribe_repeat_sec);
      }
    }

    // be nice, if we have other tasks
    delay(5);
    return true;
  }

  /// Provide addess to the service information
  DLNAServiceInfo& getService(const char* id) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getService");
    static DLNAServiceInfo no_service(false);
    for (auto& dev : devices) {
      DLNAServiceInfo& result = dev.getService(id);
      if (result) return result;
    }
    return no_service;
  }

  /// Provides the device information of the actually selected device
  DLNADeviceInfo& getDevice() {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getDevice");
    return devices[default_device_idx];
  }

  /// Provides the device information by index
  DLNADeviceInfo& getDevice(int idx) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getDevice");
    if (idx < 0 || idx >= devices.size()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Device index %d out of range", idx);
      return NO_DEVICE;
    }
    return devices[idx];
  }

  /// Provides the device for a service
  DLNADeviceInfo& getDevice(DLNAServiceInfo& service) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::getDevice");
    for (auto& dev : devices) {
      for (auto& srv : dev.getServices()) {
        if (srv == service) return dev;
      }
    }
    return NO_DEVICE;
  }

  /// Get a device for a Url
  DLNADeviceInfo& getDevice(Url location) {
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

  Vector<DLNADeviceInfo>& getDevices() { return devices; }

  /**
   * Public wrapper to build a fully-qualified URL for a device + suffix.
   * This calls the protected getUrl() helper so external helpers can reuse
   * the control point's URL normalization logic without exposing the
   * internal protected method directly.
   */
  const char* getUrl(DLNADeviceInfo& device, const char* suffix,
                       char* buffer, int len) {
    return getUrlImpl(device, suffix, buffer, len);
  }

  /// Adds a new device
  bool addDevice(DLNADeviceInfo dev) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::addDevice");
    dev.updateTimestamp();
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
  bool addDevice(Url url) {
    if (!allow_localhost && StrView(url.host()).equals("127.0.0.1")) {
      DlnaLogger.log(DlnaLogLevel::Info, "Ignoring localhost device");
      return false;
    }
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::addDevice");
    DLNADeviceInfo& device = getDevice(url);
    if (device != NO_DEVICE) {
      // device already exists
      device.setActive(true);
      return true;
    }
    // http get - incremental parse using XMLParserPrint so we don't hold
    // the whole device.xml in memory
    DLNAHttpRequest req;
    int rc = req.get(url, "text/xml");

    if (rc != 200) {
      DlnaLogger.log(DlnaLogLevel::Error, "Http get to '%s' failed with %d",
                     url.url(), rc);
      return false;
    }

    DLNADeviceInfo new_device;
    new_device.base_url = nullptr;

    XMLDeviceParser parser{new_device, registry};
    uint8_t buffer[XML_PARSER_BUFFER_SIZE];

    // Read and incrementally parse into new_device
    while (true) {
      int len = req.read(buffer, sizeof(buffer));
      if (len <= 0) break;
      parser.parse(buffer, len);
    }

    parser.finalize(new_device);

    new_device.device_url = url;
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
  void setActive(bool flag) { is_active = flag; }

  /// Checks if the scheduler is active
  bool isActive() { return is_active; }

  /// Defines if localhost devices should be allowed
  void setAllowLocalhost(bool flag) { allow_localhost = flag; }

  /// Provides the last reply
  ActionReply& getLastReply() { return reply; }

 protected:
  DLNADeviceInfo NO_DEVICE{false};
  Scheduler scheduler;
  DLNAHttpRequest* p_http = nullptr;
  IUDPService* p_udp = nullptr;
  Vector<DLNADeviceInfo> devices;
  Vector<ActionRequest> actions;
  ActionReply reply;
  XMLPrinter xml_printer;
  StringRegistry registry;
  int default_device_idx = 0;
  int msearch_repeat_ms = 10000;
  uint64_t subscribe_timeout = 0;
  int subscribe_repeat_sec = 3600;
  bool is_active = false;
  bool is_parse_device = false;
  const char* search_target;
  Url local_url;
  bool allow_localhost = false;
  HttpServer* p_http_server = nullptr;
  int http_server_port = 0;
  void* reference = nullptr;
  std::function<void(const char* nodeName, const char* text,
                     const char* attributes)>
      result_callback;
  std::function<void(void* reference, const char* sid, const char* varName,
                     const char* newValue)>
      event_callback;

  /// Attach an HttpServer so the control point can receive HTTP NOTIFY event
  /// messages. This registers a handler at the configured local URL path
  /// (see `local_url`) which will extract the SID header and body and call
  /// parseAndDispatchEvent() to dispatch property changes.
  void attachHttpServer(HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "DLNAControlPointMgr::attachHttpServer");
    p_http_server = &server;
    // register handler at the local path. If local_url is not set we use
    // a default path "/dlna/events"
    const char* path =
        StrView(local_url.url()).isEmpty() ? "/dlna/events" : local_url.path();

    void* ctx[1];
    ctx[0] = this;
    server.on(path, T_NOTIFY, notifyHandler, ctx, 1);
  }

  /// Static HTTP handler for incoming NOTIFY event callbacks. The HttpServer
  /// will pass the control point instance as context[0] so we can retrieve
  /// the instance and forward the event to parseAndDispatchEvent().
  static void notifyHandler(HttpServer* server, const char* requestPath,
                            HttpRequestHandlerLine* hl) {
    // The DLNAControlPoint instance is passed as context[0]
    DLNAControlPoint* cp = nullptr;
    if (hl && hl->contextCount > 0) {
      cp = static_cast<DLNAControlPoint*>(hl->context[0]);
    }

    if (!cp) {
      DlnaLogger.log(DlnaLogLevel::Error, "No control point found");
      server->replyNotFound();
      return;
    }

    // read headers
    HttpRequestHeader& req = server->requestHeader();
    uint8_t buffer[XML_PARSER_BUFFER_SIZE];
    XMLParserPrint xml_parser;
    Str node;
    Str text;
    Str attr;
    Vector<Str> path;
    bool active = false;
    const char* sid = req.get("SID");

    /// Parse body
    Client& client = server->client();
    while (client.available()) {
      int len = client.readBytes(buffer, sizeof(buffer));
      xml_parser.write((const uint8_t*)buffer, len);
      while (xml_parser.parse(node, path, text, attr)) {
        // execute callback
        if (active && !text.isEmpty()) {
          DlnaLogger.log(DlnaLogLevel::Info, "Event: %s = %s", node.c_str(),
                         text.c_str());
          // event callback
          if (cp->event_callback != nullptr) {
            cp->event_callback(cp->reference, sid, node.c_str(), text.c_str());
          }
        }
        /// Activate callback
        if (node.equals("e:property")) {
          active = true;
        }
      }
    }
    xml_parser.end();
    // reply OK
    server->replyOK();
  }

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

    // Not known -> fetch and add device description
    Url url{data.location.c_str()};
    selfDLNAControlPoint->addDevice(url);
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
    DlnaLogger.log(DlnaLogLevel::Info, "MSearchReply -> add device: %s",
                   data.location.c_str());
    // If the reply includes a USN we can avoid fetching multiple
    // different LOCATION URLs for the same device (different network
    // interfaces) by checking for the UDN part in known devices.
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
    bool ok = namespace_str.replace("%1", action.getServiceType());
    DlnaLogger.log(DlnaLogLevel::Debug, "ns = '%s'", namespace_str.c_str());

    // assert(ok);
    result +=
        xml_printer.printNodeBegin(action.action, namespace_str.c_str(), "u");
    for (auto arg : action.arguments) {
      result += xml_printer.printNode(arg.name, arg.value.c_str());
    }
    result += xml_printer.printNodeEnd(action.action, "u");

    result += xml_printer.printNodeEnd("Body", "s");
    result += xml_printer.printNodeEnd("Envelope", "s");
    return result;
  }

  ActionReply& postAllActions() {
    reply.clear();
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::postAllActions");
    for (auto& action : actions) {
      if (action) postAction(action);
    }
    return reply;
  }

  ActionReply& postAction(ActionRequest& action) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::postAction: %s",
                   nullStr(action.action, "(null)"));
    reply.clear();
    DLNAServiceInfo& service = *action.p_service;
    DLNADeviceInfo& device = getDevice(service);

    // Build request body
    StrPrint requestBody;
    buildActionBody(action, requestBody);

    // create SOAPACTION header value
    char act[200];
    StrView action_str{act, 200};
    action_str = "\"";
    action_str.add(action.getServiceType());
    action_str.add("#");
    action_str.add(action.action);
    action_str.add("\"");

    // crate control url
    char url_buffer[200] = {0};
    // Log service and base to help debug malformed control URLs
    DlnaLogger.log(DlnaLogLevel::Info,
                   "Service control_url: %s, device base: %s",
                   nullStr(service.control_url), nullStr(device.getBaseURL()));
    Url post_url{getUrl(device, service.control_url, url_buffer, 200)};
    DlnaLogger.log(DlnaLogLevel::Info, "POST URL computed: %s", post_url.url());

    // send HTTP POST and collect response into an XMLParserPrint so we can
    // parse incrementally
    return processActionHttpPost(post_url, requestBody, action_str.c_str());
  }

  /// Build the SOAP XML request body for the action into `out`
  void buildActionBody(ActionRequest& action, StrPrint& out) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::buildActionBody");
    xml_printer.setOutput(out);
    createXML(action);
  }

  const char* nullStr(const char* str, const char* empty = "(null)") {
    return str == nullptr ? empty : str;
  }

  const char* nullStr(Str& str, const char* empty = "(null)") {
    return str.isEmpty() ? empty : str.c_str();
  }

  // Send an HTTP POST for the given URL and request body. On success the
  /// response body is written into `responseOut` (an XMLParserPrint). Returns
  /// the HTTP rc.
  ActionReply& processActionHttpPost(Url& post_url, StrPrint& requestBody,
                                     const char* soapAction) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "DLNAControlPointMgr::processActionHttpPost");
    XMLParserPrint xml_parser;
    uint8_t buffer[XML_PARSER_BUFFER_SIZE];
    Str outNodeName;
    Vector<Str> outPath;
    Str outText;
    Str outAttributes;
    xml_parser.setExpandEncoded(true);

    // set header and post
    p_http->stop();
    p_http->request().put("SOAPACTION", soapAction);
    int rc = p_http->post(post_url, "text/xml", requestBody.c_str(),
                          requestBody.length());
    if (rc != 200) {
      p_http->stop();
      reply.setValid(false);
      DlnaLogger.log(DlnaLogLevel::Error, "Action '%s' failed with HTTP rc %d",
                     nullStr(soapAction), rc);
      return reply;
    }

    reply.setValid(true);
    reply.clear();
    // parse response
    while (p_http->client()->available()) {
      int len = p_http->client()->read(buffer, 200);
      if (len > 0) {
        // Serial.write(buffer, len);
        xml_parser.write(buffer, len);
        while (xml_parser.parse(outNodeName, outPath, outText, outAttributes)) {
          Argument arg;
          // For most nodes we only add arguments when they contain text or
          // attributes. The special "Result" node contains the DIDL-Lite
          // payload (raw XML) and must be preserved so callers (e.g. the
          // MediaServer helper) can parse returned media items.
          if (!(outText.isEmpty() && outAttributes.isEmpty()) ||
              outNodeName.equals("Result")) {
            if (!outText.isEmpty()) {
              arg.name = registry.add((char*)outNodeName.c_str());
              arg.value = outText;
              reply.addArgument(arg);
            }

            DlnaLogger.log(DlnaLogLevel::Info, "callback: '%s': %s (%s)",
                           nullStr(outNodeName), nullStr(outText),
                           nullStr(outAttributes));
            if (result_callback) {
              result_callback(nullStr(arg.name, ""), nullStr(outText, ""),
                              nullStr(outAttributes, ""));
            }
          }
        }
      }
    }
    xml_parser.end();
    return reply;
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

  /// Subscribe to changes for all device services
  bool subscribeNotifications(DLNADeviceInfo& device,
                              int timeoutSeconds = 1800) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "DLNAControlPointMgr::subscribeNotifications");
    if (p_http_server == nullptr) {
      DlnaLogger.log(
          DlnaLogLevel::Error,
          "HttpServer not defined - cannot subscribe to notifications");
      return false;
    }
    if (StrView(local_url.url()).isEmpty()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Local URL not defined");
      return false;
    }
    for (auto& service : device.getServices()) {
      if (StrView(service.event_sub_url).isEmpty()) {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "Service %s has no eventSubURL defined",
                       service.service_id);
        continue;
      }
      if (!subscribeNotifications(service, timeoutSeconds)) {
        DlnaLogger.log(DlnaLogLevel::Error, "Subscription to service %s failed",
                       service.service_id);
        return false;
      }
      DlnaLogger.log(DlnaLogLevel::Info,
                     "Subscribed to service %s successfully",
                     service.service_id);
    }
    return true;
  }

  /// Subscribe to changes for defined device service
  bool subscribeNotifications(DLNAServiceInfo& service,
                              int timoutSeconds = 1800) {
    if (p_http_server == nullptr) {
      DlnaLogger.log(
          DlnaLogLevel::Error,
          "HttpServer not defined - cannot subscribe to notifications");
      return false;
    }
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "DLNAControlPointMgr::subscribeNotifications");
    DLNADeviceInfo& device = getDevice(service);

    char url_buffer[200] = {0};
    char seconds_txt[80] = {0};
    Url url{getUrl(device, service.event_sub_url, url_buffer, 200)};
    snprintf(seconds_txt, 80, "Second-%d", timoutSeconds);
    p_http->request().put("NT", "upnp:event");
    p_http->request().put("TIMEOUT", seconds_txt);
    p_http->request().put("CALLBACK", local_url.url());
    // post subscribe
    int rc = p_http->subscribe(url);
    DlnaLogger.log(DlnaLogLevel::Info, "Http rc: %s", rc);
    if (rc == 200) {
      subscribe_timeout = millis() + (timoutSeconds * 1000);
      return true;
    }

    return false;
  }
};

}  // namespace tiny_dlna