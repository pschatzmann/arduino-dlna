#pragma once

#include "basic/Url.h"
#include "dlna/DLNADeviceInfo.h"
#include "dlna/Schedule.h"
#include "dlna/devices/DLNADevice.h"
#include "dlna/devices/DLNADeviceRequestParser.h"
#include "dlna/devices/SubscriptionMgrDevice.h"
#include "dlna/xml/XMLParserPrint.h"
#include "http/Http.h"

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
 * @author Phil Schatzmann
 */
class DLNADevice {
 public:
  /// start the
  bool begin(DLNADeviceInfo& device, IUDPService& udp, HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::begin");
    self = this;
    p_server = &server;
    p_udp = &udp;
    setDevice(device);
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

    // setup web server
    if (!setupDLNAServer(server)) {
      DlnaLogger.log(DlnaLogLevel::Error, "setupDLNAServer failed");
      return false;
    }

    // start web server
    Url url{baseUrl};
    if (!p_server->begin(url.port())) {
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

    is_active = true;
    DlnaLogger.log(DlnaLogLevel::Info, "Device successfully started");
    return true;
  }

  static SubscriptionMgrDevice* getSubscriptionMgr() {
    return self ? &self->subscription_mgr : nullptr;
  }

  /// Stops the processing and releases the resources
  void end() {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::end");
    p_server->end();

    // send 3 bye messages
    PostByeSchedule* bye = new PostByeSchedule(*p_device);
    bye->repeat_ms = 800;
    scheduler.add(bye);

    // execute scheduler for 2 seconds
    uint64_t end = millis() + 2000;
    while (millis() < end) {
      scheduler.execute(*p_udp);
    }

    is_active = false;
  }

  /// call this method in the Arduino loop as often as possible
  bool loop() {
    if (!is_active) return false;
    // Platform-specific periodic diagnostics (e.g. ESP32 memory logging)
    logMemoryIfNeeded();

    // handle server requests
    bool rc = p_server->doLoop();
    DlnaLogger.log(DlnaLogLevel::Debug, "server %s", rc ? "true" : "false");

    if (isSchedulerActive()) {
      processUdpAndScheduler();
    } else {
      DlnaLogger.log(DlnaLogLevel::Warning, "scheduler inactive");
    }

    // be nice, if we have other tasks
    delay(DLNA_LOOP_DELAY_MS);

    return true;
  }

  /// Provide addess to the service information
  DLNAServiceInfo& getService(const char* id) {
    return p_device->getService(id);
  }

  /// Get Service by subscription ns abbrev
  DLNAServiceInfo& getServiceByAbbrev(const char* abbrev) {
    return p_device->getServiceByAbbrev(abbrev);
  }

  /// Publish a property change to subscribers of the service identified by
  /// the subscription namespace abbreviation (e.g. "AVT", "RCS"). This
  /// forwards the request to the SubscriptionMgrDevice instance.
  void publishProperty(const char* serviceAbbrev, const char* changeTag) {
    SubscriptionMgrDevice* mgr = tiny_dlna::DLNADevice::getSubscriptionMgr();
    if (!mgr) {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "publishProperty: No SubscriptionMgrDevice available");
      return;
    }
    DLNAServiceInfo& serviceInfo = getServiceByAbbrev(serviceAbbrev);
    if (!serviceInfo) {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "publishProperty: No service info available for %s", serviceAbbrev);
      return;
    }
    mgr->publishProperty(serviceInfo, changeTag);
  }

  /// Provides the device
  DLNADeviceInfo& getDevice() { return *p_device; }

  /// We can activate/deactivate the scheduler
  void setSchedulerActive(bool flag) { scheduler.setActive(flag); }

  /// Checks if the scheduler is active
  bool isSchedulerActive() { return scheduler.isActive(); }

  /// Repeat the post-alive messages (default: 0 = no repeat). Call this method
  /// before calling begin!
  void setPostAliveRepeatMs(uint32_t ms) { post_alive_repeat_ms = ms; }

  /// Static handler for SUBSCRIBE/UNSUBSCRIBE requests on service event URLs
  static void eventSubscriptionHandler(HttpServer* server,
                                       const char* requestPath,
                                       HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaLogLevel::Debug, "eventSubscriptionHandler");
    bool is_subscribe = false;
    // context[0] contains DLNADeviceInfo*
    DLNADeviceInfo* device = (DLNADeviceInfo*)(hl->context[0]);
    if (!device) {
      server->replyNotFound();
      return;
    }
    // header parsing
    HttpRequestHeader& req = server->requestHeader();
    // SUBSCRIBE handling
    if (req.method() == T_SUBSCRIBE) {
      is_subscribe = true;
      const char* cb = req.get("CALLBACK");
      const char* timeout = req.get("TIMEOUT");
      Str cbStr;
      if (cb) {
        cbStr = cb;
        cbStr.replace("<", "");
        cbStr.replace(">", "");
      }
      uint32_t tsec = 1800;
      if (timeout && StrView(timeout).startsWith("Second-")) {
        tsec = atoi(timeout + 7);
      }
      // Use the service event path as key for subscriptions
      const char* svcKey = requestPath;
      if (DLNADevice::self) {
        Str sid = DLNADevice::self->subscription_mgr.subscribe(
            svcKey, cbStr.c_str(), tsec);
        server->replyHeader().setValues(200, "OK");
        server->replyHeader().put("SID", sid.c_str());
        server->replyHeader().put("TIMEOUT", "Second-1800");
        server->replyHeader().write(server->client());
        server->replyOK();
        return;
      }
      server->replyNotFound();
      return;
    }
    // UNSUBSCRIBE: handle via SID header
    if (req.method() == T_UNSUBSCRIBE) {
      // Some stacks use POST for unsubscribe; try to handle SID
      const char* sid = req.get("SID");
      if (sid && DLNADevice::self) {
        bool ok =
            DLNADevice::self->subscription_mgr.unsubscribe(requestPath, sid);
        if (ok) {
          server->replyOK();
          return;
        }
      }
    }
    // default reply OK
    server->replyOK();
  }

  /// Parses the SOAP content of a DLNA action request
  static void parseActionRequest(HttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl,
                                 ActionRequest& action) {
    DlnaLogger.log(DlnaLogLevel::Info, "parseActionRequest");

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

    while (client.available() > 0) {
      size_t len = client.readBytes(buffer, sizeof(buffer));
      xp.write((const uint8_t*)buffer, len);

      while (xp.parse(outNodeName, outPath, outText, outAttributes)) {
        if (is_attribute) {
          const char* argName = registry.add((char*)outNodeName.c_str());
          action.addArgument(argName, outText.c_str());

          continue;
        }
        if (is_action) {
          is_action = false;
          is_attribute = true;
          action.action = registry.add((char*)outNodeName.c_str());
          DlnaLogger.log(DlnaLogLevel::Info, "action: %s", action.action);
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
  }

  StringRegistry& getStringRegistry() { return registry; }

 protected:
  bool is_active = false;
  uint32_t post_alive_repeat_ms = 0;
  Scheduler scheduler;
  SubscriptionMgrDevice subscription_mgr;
  DLNADeviceRequestParser parser;
  DLNADeviceInfo* p_device = nullptr;
  IUDPService* p_udp = nullptr;
  HttpServer* p_server = nullptr;
  inline static StringRegistry registry;
  inline static DLNADevice* self = nullptr;

  void setDevice(DLNADeviceInfo& device) { p_device = &device; }

  /// Periodic platform-specific diagnostics. Kept as a separate method so
  /// it can be tested or stubbed easily. Currently logs ESP32 heap/psram
  /// every 10s.
  void logMemoryIfNeeded() {
#ifdef ESP32
    static uint32_t last_mem_log = 0;
    const uint32_t MEM_LOG_INTERVAL_MS = 10000;
    uint32_t now = millis();
    if ((uint32_t)(now - last_mem_log) >= MEM_LOG_INTERVAL_MS) {
      last_mem_log = now;
      DlnaLogger.log(DlnaLogLevel::Info,
                     "Mem: freeHeap=%u freePsram=%u  / Scheduler: size=%d "
                     "active=%s / StrRegistry: count=%d size=%d",
                     (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(),
                     scheduler.size(), isSchedulerActive() ? "true" : "false",
                     registry.count(), registry.size());
    }
#endif
    
  }

  /// Process incoming UDP and execute scheduled replies. The method will
  /// respect `scheduler_active` when calling `Scheduler::execute` so
  /// periodic schedules can be silenced while still allowing on-demand
  /// replies (e.g. MSearchReply) to be handled.
  void processUdpAndScheduler() {
    // process UDP requests
    DlnaLogger.log(DlnaLogLevel::Debug, "processUdpAndScheduler");
    RequestData req = p_udp->receive();
    if (req) {
      Schedule* schedule = parser.parse(*p_device, req);
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
    parser.addMSearchST(p_device->getUDN());
    parser.addMSearchST(p_device->getDeviceType());
    return true;
  }

  /// Schedule PostAlive messages
  bool setupScheduler() {
    DlnaLogger.log(DlnaLogLevel::Debug, "setupScheduler");
    // schedule post alive messages: Usually repeated 2 times (because UDP
    // messages might be lost)
    PostAliveSchedule* postAlive =
        new PostAliveSchedule(*p_device, post_alive_repeat_ms);
    PostAliveSchedule* postAlive1 =
        new PostAliveSchedule(*p_device, post_alive_repeat_ms);
    postAlive1->time = millis() + 100;
    scheduler.add(postAlive);
    scheduler.add(postAlive1);
    return true;
  }

  /// set up Web Server to handle Service Addresses
  virtual bool setupDLNAServer(HttpServer& srv) {
    DlnaLogger.log(DlnaLogLevel::Debug, "setupDLNAServer");
    char buffer[DLNA_MAX_URL_LEN] = {0};
    StrView url(buffer, DLNA_MAX_URL_LEN);

    // add device url to server
    const char* device_path = p_device->getDeviceURL().path();
    const char* prefix = p_device->getBaseURL();

    DlnaLogger.log(DlnaLogLevel::Info, "Setting up device path: %s",
                   device_path);
    void* ref[] = {p_device};

    if (!StrView(device_path).isEmpty()) {
      p_server->rewrite("/", device_path);
      p_server->rewrite("/dlna/device.xml", device_path);
      p_server->rewrite("/index.html", device_path);
      p_server->on(device_path, T_GET, deviceXMLCallback, ref, 1);
    }

    // Register icon and privide favicon.ico
    Icon icon = p_device->getIcon();
    if (icon.icon_data != nullptr) {
      char tmp[DLNA_MAX_URL_LEN];
      // const char* icon_path = url.buildPath(prefix, icon.icon_url);
      p_server->on(icon.icon_url, T_GET, icon.mime,
                   (const uint8_t*)icon.icon_data, icon.icon_size);
      p_server->on("/favicon.ico", T_GET, icon.mime,
                   (const uint8_t*)icon.icon_data, icon.icon_size);
    }

    // Register Service URLs
    for (DLNAServiceInfo& service : p_device->getServices()) {
      p_server->on(service.scpd_url, T_GET, service.scp_cb, ref, 1);
      p_server->on(service.control_url, T_POST, service.control_cb, ref, 1);
      // register event subscription handler - wrappers for
      // SUBSCRIBE/UNSUBSCRIBE
      p_server->on(service.event_sub_url, T_SUBSCRIBE, eventSubscriptionHandler,
                   ref, 1);
    }

    return true;
  }

  /// callback to provide device XML
  static void deviceXMLCallback(HttpServer* server, const char* requestPath,
                                HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaLogLevel::Debug, "deviceXMLCallback");

    DLNADeviceInfo* device_xml = (DLNADeviceInfo*)(hl->context[0]);
    assert(device_xml != nullptr);
    if (device_xml != nullptr) {
      Client& client = server->client();
      assert(&client != nullptr);
      DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "DeviceXML");
      server->replyHeader().setValues(200, "SUCCESS");
      server->replyHeader().put(CONTENT_TYPE, "text/xml");
      server->replyHeader().put(CONNECTION, CON_KEEP_ALIVE);
      server->replyHeader().write(client);

      // print xml result
      device_xml->print(client);
      server->endClient();
    } else {
      DlnaLogger.log(DlnaLogLevel::Error, "DLNADevice is null");
      server->replyNotFound();
    }
  }
};

}  // namespace tiny_dlna