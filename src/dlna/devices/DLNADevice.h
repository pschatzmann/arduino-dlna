#pragma once

#include <functional>

#include "basic/Printf.h"
#include "basic/Url.h"
#include "dlna/DLNADeviceInfo.h"
#include "dlna/Schedule.h"
#include "dlna/devices/IDevice.h"
#include "dlna/devices/DLNADeviceRequestParser.h"
#include "dlna/devices/SubscriptionMgrDevice.h"
#include "dlna/xml/XMLParserPrint.h"
#include "http/Http.h"
#include "http/Server/IHttpServer.h"
#include "udp/IUDPService.h"

namespace tiny_dlna {

/**
 * @brief Setup of a Basic DLNA Device service. The device registers itself to
 * the network and answers to the DLNA queries and requests. A DLNA device uses
 * UDP, Http, XML and Soap to discover and manage a service, so there is quite
 * some complexity involved:
 * - We handle the UDP communication via a Scheduler and a Request Parser
 * - We handle the Http request with the help of the TinyHttp Server
 * - The XML service descriptions can be stored as char arrays in progmem or
 * generated dynamically with the help of the XMLPrinter class.
 *
 * The template parameter selects the HTTP client implementation (for example
 * WiFiClient) that will be used by all internal helpers; choose the one that
 * matches your platform.
 *
 * @tparam ClientType Arduino `Client` implementation used for outbound HTTP
 *         traffic (e.g. `WiFiClient`, `EthernetClient`).
 *
 * @author Phil Schatzmann
 */
template <typename ClientType>
class DLNADevice : public IDevice {
 public:
  DLNADevice() {
    // distribute initial subscription active flag to subscription manager
    setSubscriptionsActive(isSubscriptionsActive());
  }
  /// start the
  bool begin(DLNADeviceInfo& device, IUDPService& udp,
             IHttpServer& server) override {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::begin");
    server.setReference(this);
    p_server = &server;
    p_udp = &udp;
    setDeviceInfo(device);
    setupParser();

    // check base url
    const char* baseUrl = device.getBaseURL();
    DlnaLogger.log(DlnaLogLevel::Info, "base URL: %s", baseUrl);

    if (StrView(device.getBaseURL()).contains("localhost")) {
      DlnaLogger.log(DlnaLogLevel::Error, "invalid base address: %s", baseUrl);
      return false;
    }

    // setup device
    device.setupServices(server, udp);

    // ensure services reflect the current subscriptions-active flag
    // (some callers rely on subscriptions being disabled by default)
    setSubscriptionsActive(isSubscriptionsActive());

    // setup web server
    if (!setupDLNAServer(server)) {
      DlnaLogger.log(DlnaLogLevel::Error, "setupDLNAServer failed");
      return false;
    }

    // start web server
    Url url{baseUrl};
    if (!p_server->begin()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Server failed");
      return false;
    }

    // setup UDP
    if (!p_udp->begin(DLNABroadcastAddress)) {
      DlnaLogger.log(DlnaLogLevel::Error, "UDP begin failed");
      return false;
    }

    // setup scheduler
    if (!setupScheduler()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Scheduler failed");
      return false;
    }
    // initialize scheduling timers so the scheduled tasks run immediately
    uint64_t now = millis();
    // set next timeouts to now so the first loop triggers the tasks
    next_scheduler_timeout_ms = now;
    next_subscriptions_timeout_ms = now;

    is_active = true;
    DlnaLogger.log(DlnaLogLevel::Info, "Device successfully started");
    return true;
  }

  ISubscriptionMgrDevice& getSubscriptionMgr() override {
    return subscription_mgr;
  }

  /// Stops the processing and releases the resources
  void end() override {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::end");
    p_server->end();

    // send 3 bye messages
    PostByeSchedule* bye = new PostByeSchedule(*p_device_info);
    bye->repeat_ms = 800;
    scheduler.add(bye);

    // execute scheduler for 2 seconds
    uint64_t end = millis() + 2000;
    while (millis() < end) {
      scheduler.execute(*p_udp);
    }

    // end subscription manager
    getSubscriptionMgr().end();

    is_active = false;
  }

  /// call this method in the Arduino loop as often as possible
  bool loop() override {
    if (!is_active) return false;
    // Platform-specific periodic diagnostics (e.g. ESP32 memory logging)
    logMemoryIfNeeded();

    // handle server requests
    bool rc = loopServer();

    // Use millisecond-based intervals for scheduling so callers can set
    // real-time intervals instead of loop-counts.
    uint64_t now = millis();
    if (isSchedulerActive() && now >= next_scheduler_timeout_ms) {
      processUdpAndScheduler();
      // schedule next run
      next_scheduler_timeout_ms = now + scheduler_interval_ms;
    }

    // deliver any queued subscription notifications (if enabled)
    if (isSubscriptionsActive() && now >= next_subscriptions_timeout_ms) {
      subscription_mgr.publish();
      // schedule next run
      next_subscriptions_timeout_ms = now + subscriptions_interval_ms;
    }

    // be nice, if we have other tasks
    delay(DLNA_LOOP_DELAY_MS);
    return true;
  }

  bool loopServer() override {
    if (!is_active) return false;
    bool rc = p_server->doLoop();
    DlnaLogger.log(DlnaLogLevel::Debug, "server %s", rc ? "true" : "false");
    return rc;
  }

  /// Provide addess to the service information
  DLNAServiceInfo& getService(const char* id) override {
    return p_device_info->getService(id);
  }

  /// Get Service by subscription ns abbrev
  DLNAServiceInfo& getServiceByAbbrev(const char* abbrev) override {
    return p_device_info->getServiceByAbbrev(abbrev);
  }

  /// Find a service by its event subscription URL (returns nullptr if not
  /// found). This encapsulates the lookup that was previously done inline
  /// in the static subscription handler.
  DLNAServiceInfo* getServiceByEventPath(
      const char* requestPath) override {
    if (p_device_info == nullptr || requestPath == nullptr) return nullptr;
    for (DLNAServiceInfo& s : p_device_info->getServices()) {
      if (StrView(s.event_sub_url).equals(requestPath)) {
        return &s;
      }
    }
    return nullptr;
  }

  /// Record a state variable change for subscription notifications
  void addChange(const char* serviceAbbrev,
                 std::function<size_t(Print&, void*)> changeWriter,
                 void* ref) override {
    auto& mgr = getSubscriptionMgr();
    DLNAServiceInfo& serviceInfo = getServiceByAbbrev(serviceAbbrev);
    if (!serviceInfo) {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "addChange: No service info available for %s",
                     serviceAbbrev);
      return;
    }
    mgr.addChange(serviceInfo, changeWriter, ref);
  }

  /// Provides the device
  DLNADeviceInfo& getDeviceInfo() override { return *p_device_info; }

  /// We can activate/deactivate the scheduler
  void setSchedulerActive(bool flag) override { scheduler.setActive(flag); }

  /// Checks if the scheduler is active
  bool isSchedulerActive() override { return scheduler.isActive(); }

  /// Repeat the post-alive messages (default: 0 = no repeat). Call this method
  /// before calling begin!
  void setPostAliveRepeatMs(uint32_t ms) override {
    post_alive_repeat_ms = ms;
  }


  /// Enable or disable subscription notifications: call before begin
  void setSubscriptionsActive(bool flag) override {
    is_subscriptions_active = flag;
    if (p_device_info != nullptr) {
      p_device_info->setSubscriptionActive(flag);
    }
    getSubscriptionMgr().setSubscriptionsActive(flag);
  }

  /// Check if subscription notifications are active
  bool isSubscriptionsActive() const override {
    return is_subscriptions_active;
  }

  /// Parses the SOAP content of a DLNA action request
  static void parseActionRequest(IHttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl,
                                 ActionRequest& action) {
    DlnaLogger.log(DlnaLogLevel::Info, "parseActionRequest");
    auto start = millis();
    XMLParserPrint xp;
    xp.setExpandEncoded(true);

    Str outNodeName;
    Vector<Str> outPath;
    Str outText;
    Str outAttributes;
    bool is_attribute = false;
    bool is_action = false;
    Str actionName;
    char buffer[XML_PARSER_BUFFER_SIZE];
    Client& client = server->client();

    client.setTimeout(20);
    while (true) {
      size_t len = client.readBytes(buffer, sizeof(buffer));
      if (len == 0) break;
      xp.write((const uint8_t*)buffer, len);

      while (xp.parse(outNodeName, outPath, outText, outAttributes)) {
        if (is_attribute) {
          const char* argName = outNodeName.c_str();
          action.addArgument(argName, outText.c_str());
          continue;
        }
        if (is_action) {
          is_action = false;
          is_attribute = true;
          action.setAction(outNodeName.c_str());
          DlnaLogger.log(DlnaLogLevel::Info, "action: %s", action.getAction());
          continue;
        }
        // skip SOAP envelope wrappers
        if (outNodeName.equals("s:Envelope") || outNodeName.equals("Body")) {
          continue;
        }
        if (outNodeName.equals("s:Body")) {
          is_action = true;
        }
      }
    }
    xp.end();
    DlnaLogger.log(DlnaLogLevel::Info, "Parse took %d ms", millis() - start);
  }

  /**
   * @brief Builds a standard SOAP reply envelope
   *
   * This helper prints a SOAP envelope and calls the provided valuesWriter
   * to emit the inner XML fragment. The valuesWriter receives the output
   * Print and a void* ref for caller-provided context and shall return the
   * number of bytes written.
   */
  static size_t printReplyXML(
      Print& out, const char* replyName, const char* serviceId,
      std::function<size_t(Print&, void*)> valuesWriter = nullptr,
      void* ref = nullptr) {
    XMLPrinter xp(out);
    size_t result = 0;
    result += xp.printNodeBegin(
        "s:Envelope", "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"");
    result += xp.printNodeBegin("s:Body");
    result += xp.printf("<u:%s xmlns:u=\"urn:schemas-upnp-org:service:%s:1\">",
                        replyName, serviceId);

    // e.g.<u:return>Stop,Pause,Next,Seek</u:return> for
    // QueryStateVariableResponse
    if (valuesWriter) {
      result += valuesWriter(out, ref);
    }

    result += xp.printf("</u:%s>", replyName);
    result += xp.printNodeEnd("s:Body");
    result += xp.printNodeEnd("s:Envelope");
    return result;
  }
  /**
   * Parameterized variants that allow callers to pass dynamic values from
   * devices (e.g. source/sink protocol lists or current connection IDs).
   */
  static size_t replyGetProtocolInfo(Print& out, const char* source = "",
                                     const char* sink = "") {
    return printReplyXML(out, "GetProtocolInfoResponse", "ConnectionManager",
                         [source, sink](Print& o, void* ref) -> size_t {
                           (void)ref;
                           size_t written = 0;
                           written += o.print("<Source>");
                           written += o.print(StrView(source).c_str());
                           written += o.print("</Source>");
                           written += o.print("<Sink>");
                           written += o.print(StrView(sink).c_str());
                           written += o.print("</Sink>");
                           return written;
                         });
  }

  static size_t replyGetCurrentConnectionIDs(Print& out, const char* ids) {
    return printReplyXML(
        out, "GetCurrentConnectionIDsResponse", "ConnectionManager",
        [ids](Print& o, void* ref) -> size_t {
          (void)ref;
          size_t written = 0;
          // UPnP spec uses "CurrentConnectionIDs" as the
          // response element name
          written += o.print("<CurrentConnectionIDs>");
          written += o.print(StrView(ids ? ids : "0").c_str());
          written += o.print("</CurrentConnectionIDs>");
          return written;
        });
  }

  static size_t replyGetCurrentConnectionInfo(Print& out,
                                              const char* protocolInfo,
                                              const char* connectionID,
                                              const char* direction) {
    return printReplyXML(
        out, "GetCurrentConnectionInfoResponse", "ConnectionManager",
        [protocolInfo, connectionID, direction](Print& o, void* ref) -> size_t {
          (void)ref;
          size_t written = 0;
          // Write directly to out to avoid using a fixed-size intermediate
          // buffer.
          written += o.print("<RcsID>0</RcsID>");
          written += o.print("<AVTransportID>0</AVTransportID>");
          written += o.print("<ProtocolInfo>");
          written += o.print(StrView(protocolInfo).c_str());
          written += o.print("</ProtocolInfo>");
          written += o.print("<PeerConnectionManager></PeerConnectionManager>");
          written += o.print("<PeerConnectionID>");
          written += o.print(StrView(connectionID).c_str());
          written += o.print("</PeerConnectionID>");
          written += o.print("<Direction>");
          written += o.print(StrView(direction).c_str());
          written += o.print("</Direction>");
          written += o.print("<Status>OK</Status>");
          return written;
        });
  }

  /// Static handler for SUBSCRIBE/UNSUBSCRIBE requests on service event URLs
  static bool handleSubscription(IHttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl,
                                 bool& is_subscribe) {
    // Dispatch to dedicated handlers for subscribe/unsubscribe to keep the
    // logic separated and easier to test.
    is_subscribe = false;
    HttpRequestHeader& req = server->requestHeader();
    if (req.method() == T_SUBSCRIBE) {
      is_subscribe = true;
      return handleSubscribe(server, requestPath, hl);
    }
    if (req.method() == T_UNSUBSCRIBE) {
      return handleUnsubscribe(server, requestPath, hl);
    }

    // default reply OK for other methods
    server->replyError(501, "Unsupported Method");
    return false;
  }

  /// Sets a reference pointer that can be used to associate application
  void setReference(void* ref) override { reference = ref; }

  /// Gets the reference pointer
  void* getReference() override { return reference; }

 protected:
  bool is_active = false;
  bool is_subscriptions_active = true;
  uint32_t post_alive_repeat_ms = 0;
  Scheduler scheduler;
  SubscriptionMgrDevice<ClientType> subscription_mgr;
  DLNADeviceRequestParser parser;
  DLNADeviceInfo* p_device_info = nullptr;
  IUDPService* p_udp = nullptr;
  IHttpServer* p_server = nullptr;
  void* reference = nullptr;

  // Millisecond-based scheduling (defaults derived from existing loop
  // constants) Use millisecond-based defaults from configuration (macros ending
  // with _MS)
  uint64_t scheduler_interval_ms = (uint64_t)DLNA_RUN_SCHEDULER_EVERY_MS;
  uint64_t subscriptions_interval_ms =
      (uint64_t)DLNA_RUN_SUBSCRIPTIONS_EVERY_MS;
  // store the next absolute timeout (millis) when the task should run
  uint64_t next_scheduler_timeout_ms = 0;
  uint64_t next_subscriptions_timeout_ms = 0;

  void setDeviceInfo(DLNADeviceInfo& device) {
    p_device_info = &device;
    device.setSubscriptionActive(isSubscriptionsActive());
  }

  /// Set scheduler interval in milliseconds (default =
  /// DLNA_RUN_SCHEDULER_EVERY_MS * DLNA_LOOP_DELAY_MS) Set scheduler interval
  /// in milliseconds (default = DLNA_RUN_SCHEDULER_EVERY_MS)
  void setSchedulerIntervalMs(uint32_t ms) { scheduler_interval_ms = ms; }
  uint32_t getSchedulerIntervalMs() const {
    return (uint32_t)scheduler_interval_ms;
  }

  /// Set subscription publish interval in milliseconds (default =
  /// DLNA_RUN_SUBSCRIPTIONS_EVERY_MS * DLNA_LOOP_DELAY_MS) Set subscription
  /// publish interval in milliseconds (default =
  /// DLNA_RUN_SUBSCRIPTIONS_EVERY_MS)
  void setSubscriptionsIntervalMs(uint32_t ms) {
    subscriptions_interval_ms = ms;
  }
  uint32_t getSubscriptionsIntervalMs() const {
    return (uint32_t)subscriptions_interval_ms;
  }

  /// Periodic platform-specific diagnostics. Kept as a separate method so
  /// it can be tested or stubbed easily. Currently logs ESP32 heap/psram
  /// every 10s.
  void logMemoryIfNeeded() {
    static uint64_t last_mem_log = 0;
    const uint64_t MEM_LOG_INTERVAL_MS = 10000;
    uint64_t now = millis();
    if ((uint64_t)(now - last_mem_log) >= MEM_LOG_INTERVAL_MS) {
      // update timestamp for next interval on all platforms
      last_mem_log = now;
#ifdef ESP32
      DlnaLogger.log(DlnaLogLevel::Info,
                     "Mem: freeHeap=%u freePsram=%u  / Scheduler: size=%d "
                     "active=%s / StrRegistry: count=%d size=%d",
                     (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(),
                     scheduler.size(), isSchedulerActive() ? "true" : "false",
                     registry.count(), registry.size());
#endif
    }
  }

  /// Process incoming UDP and execute scheduled replies.
  void processUdpAndScheduler() {
    // process UDP requests
    DlnaLogger.log(DlnaLogLevel::Debug, "processUdpAndScheduler");
    RequestData req = p_udp->receive();
    if (req) {
      Schedule* schedule = parser.parse(*p_device_info, req);
      if (schedule != nullptr) {
        scheduler.add(schedule);
      }
    }

    // execute scheduled udp replys
    scheduler.execute(*p_udp);
  }

  /// MSearch requests reply to upnp:rootdevice and the device type defined
  /// in the device
  bool setupParser() {
    DlnaLogger.log(DlnaLogLevel::Debug, "setupParser");
    parser.addMSearchST("upnp:rootdevice");
    parser.addMSearchST("ssdp:all");
    parser.addMSearchST(p_device_info->getUDN());
    parser.addMSearchST(p_device_info->getDeviceType());
    return true;
  }

  /// Schedule PostAlive messages
  bool setupScheduler() {
    DlnaLogger.log(DlnaLogLevel::Debug, "setupScheduler");
    // schedule post alive messages: Usually repeated 2 times (because UDP
    // messages might be lost)
    PostAliveSchedule* postAlive =
        new PostAliveSchedule(*p_device_info, post_alive_repeat_ms);
    PostAliveSchedule* postAlive1 =
        new PostAliveSchedule(*p_device_info, post_alive_repeat_ms);
    postAlive1->time = millis() + 100;
    scheduler.add(postAlive);
    scheduler.add(postAlive1);
    return true;
  }

  /// Validates that all required service URLs and callbacks are defined
  bool validateServiceInfo(const DLNAServiceInfo& service) {
    bool has_error = false;

    if (!service.scpd_url || !*service.scpd_url) {
      DlnaLogger.log(DlnaLogLevel::Error, "Service missing scpd_url");
      has_error = true;
    }
    if (!service.control_url || !*service.control_url) {
      DlnaLogger.log(DlnaLogLevel::Error, "Service missing control_url");
      has_error = true;
    }
    if (!service.event_sub_url || !*service.event_sub_url) {
      DlnaLogger.log(DlnaLogLevel::Error, "Service missing event_sub_url");
      has_error = true;
    }
    if (!service.scp_cb) {
      DlnaLogger.log(DlnaLogLevel::Error, "Service missing scp_cb for %s",
                     service.scpd_url ? service.scpd_url : "(null)");
      has_error = true;
    }
    if (!service.control_cb) {
      DlnaLogger.log(DlnaLogLevel::Error, "Service missing control_cb for %s",
                     service.control_url ? service.control_url : "(null)");
      has_error = true;
    }
    if (!service.event_sub_cb) {
      DlnaLogger.log(DlnaLogLevel::Error, "Service missing event_sub_cb for %s",
                     service.event_sub_url ? service.event_sub_url : "(null)");
      has_error = true;
    }

    if (has_error) {
      DlnaLogger.log(DlnaLogLevel::Error,
                     "Service validation failed - missing URLs/callbacks");
    }

    return !has_error;
  }

  /// set up Web Server to handle Service Addresses
  virtual bool setupDLNAServer(IHttpServer& srv) {
    DlnaLogger.log(DlnaLogLevel::Debug, "setupDLNAServer");
    char buffer[DLNA_MAX_URL_LEN] = {0};
    StrView url(buffer, DLNA_MAX_URL_LEN);

    // add device url to server
    const char* device_path = p_device_info->getDeviceURL().path();
    const char* prefix = p_device_info->getBaseURL();

    DlnaLogger.log(DlnaLogLevel::Info, "Setting up device path: %s",
                   device_path);
    void* ref[] = {p_device_info};

    if (!StrView(device_path).isEmpty()) {
      p_server->rewrite("/", device_path);
      p_server->rewrite("/dlna/device.xml", device_path);
      p_server->rewrite("/device.xml", device_path);
      p_server->rewrite("/index.html", device_path);
      p_server->on(device_path, T_GET, deviceXMLCallback, ref, 1);
    }

    // Register icon and privide favicon.ico
    Icon icon = p_device_info->getIcon();
    if (icon.icon_data != nullptr) {
      char tmp[DLNA_MAX_URL_LEN];
      // const char* icon_path = url.buildPath(prefix, icon.icon_url);
      p_server->on(icon.icon_url, T_GET, icon.mime,
                   (const uint8_t*)icon.icon_data, icon.icon_size);
      p_server->on("/favicon.ico", T_GET, icon.mime,
                   (const uint8_t*)icon.icon_data, icon.icon_size);
    }

    for (DLNAServiceInfo& service : p_device_info->getServices()) {
      // Validate service before registering
      if (!validateServiceInfo(service)) {
        DlnaLogger.log(
            DlnaLogLevel::Error,
            "Skipping service registration due to validation failure");
        continue;
      }

      p_server->on(service.scpd_url, T_GET, service.scp_cb);
      p_server->on(service.control_url, T_POST, service.control_cb);
      p_server->on(service.event_sub_url, T_SUBSCRIBE, service.event_sub_cb);
      p_server->on(service.event_sub_url, T_UNSUBSCRIBE, service.event_sub_cb);
      p_server->on(service.event_sub_url, T_POST, service.event_sub_cb);
    }

    return true;
  }

  /// callback to provide device XML
  static void deviceXMLCallback(IHttpServer* server, const char* requestPath,
                                HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaLogLevel::Debug, "deviceXMLCallback");

    DLNADeviceInfo* device_xml = (DLNADeviceInfo*)(hl->context[0]);
    assert(device_xml != nullptr);
    if (device_xml != nullptr) {
      DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "DeviceXML");
      // Use server->reply with a callback so the Content-Length is computed
      // and headers are written correctly before streaming the body.
      server->reply(
          "text/xml",
          [](Print& out, void* ref) -> size_t {
            return ((DLNADeviceInfo*)ref)->print(out, ref);
          },
          200, "SUCCESS", device_xml);
    } else {
      DlnaLogger.log(DlnaLogLevel::Error, "DLNADevice is null");
      server->replyNotFound();
    }
  }

  /// Handle SUBSCRIBE requests 
  static bool handleSubscribe(IHttpServer* server, const char* requestPath,
                              HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaLogLevel::Debug, "handleSubscribe");
    auto* device = static_cast<IDevice*>(server->getReference());
    assert(device != nullptr);
    DLNAServiceInfo* svc = device->getServiceByEventPath(requestPath);
    assert(svc != nullptr);

    // Delegate all HTTP processing to SubscriptionMgrDevice
    return device->getSubscriptionMgr().processSubscribeRequest(*server, *svc);
  }

  /// Handle UNSUBSCRIBE requests 
  static bool handleUnsubscribe(IHttpServer* server, const char* requestPath,
                                HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaLogLevel::Debug, "handleUnsubscribe");
    auto* device = static_cast<IDevice*>(server->getReference());
    assert(device != nullptr);
    DLNAServiceInfo* svc = device->getServiceByEventPath(requestPath);
    assert(svc != nullptr);

    // Delegate all HTTP processing to SubscriptionMgrDevice
    return device->getSubscriptionMgr().processUnsubscribeRequest(*server,
                                                                  *svc);
  }
};

}  // namespace tiny_dlna