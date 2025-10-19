#pragma once

#include "DLNADevice.h"
#include "DLNADeviceMgr.h"
#include "DLNADeviceRequestParser.h"
#include "Schedule.h"
#include "SubscriptionMgr.h"
#include "basic/Url.h"
#include "http/HttpServer.h"

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
 * @author Phil Schatzmann
 */
class DLNADeviceMgr {
 public:
  /// start the
  bool begin(DLNADevice& device, IUDPService& udp, HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::begin");

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
    if (!device.begin()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Device begin failed");
      return false;
    }

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

    if (!setupScheduler()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Scheduler failed");
      return false;
    }

    is_active = true;
    DlnaLogger.log(DlnaLogLevel::Info, "Device successfully started");
    // expose singleton for subscription publishing
    instance = this;
    return true;
  }

  static SubscriptionMgr* getSubscriptionMgr() {
    return instance ? &instance->subscriptionMgr : nullptr;
  }

  /// Stops the processing and releases the resources
  void end() {
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

    // handle server requests
    bool rc = p_server->doLoop();
    DlnaLogger.log(DlnaLogLevel::Debug, "server %s", rc ? "true" : "false");

    if (isSchedulerActive()) {
      // process UDP requests
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

    // be nice, if we have other tasks
    p_device->loop();

    return true;
  }

  /// Provide addess to the service information
  DLNAServiceInfo getService(const char* id) {
    return p_device->getService(id);
  }

  /// Provides the device
  DLNADevice& getDevice() { return *p_device; }

  /// We can activate/deactivate the scheduler
  void setSchedulerActive(bool flag) { scheduler_active = flag; }

  /// Checks if the scheduler is active
  bool isSchedulerActive() { return scheduler_active; }

  /// Repeat the post-alive messages (default: 0 = no repeat). Call this method
  /// before calling begin!
  void setPostAliveRepeatMs(uint32_t ms) { post_alive_repeat_ms = ms; }

 protected:
  Scheduler scheduler;
  DLNADeviceRequestParser parser;
  IUDPService* p_udp = nullptr;
  DLNADevice* p_device = nullptr;
  HttpServer* p_server = nullptr;
  bool is_active = false;
  bool scheduler_active = true;
  uint32_t post_alive_repeat_ms = 0;
  SubscriptionMgr subscriptionMgr;
  inline static DLNADeviceMgr* instance = nullptr;

  void setDevice(DLNADevice& device) { p_device = &device; }

  /// MSearch requests reply to upnp:rootdevice and the device type defined in
  /// the device
  bool setupParser() {
    parser.addMSearchST("upnp:rootdevice");
    parser.addMSearchST("ssdp:all");
    parser.addMSearchST(p_device->getUDN());
    parser.addMSearchST(p_device->getDeviceType());
    return true;
  }

  /// Schedule PostAlive messages
  bool setupScheduler() {
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
      auto eventHandler = [](HttpServer* server, const char* requestPath,
                             HttpRequestHandlerLine* hl) {
        // context[0] contains DLNADevice*
        DLNADevice* device = (DLNADevice*)(hl->context[0]);
        if (!device) {
          server->replyNotFound();
          return;
        }
        // header parsing
        HttpRequestHeader& req = server->requestHeader();
        // SUBSCRIBE handling
        if (req.method() == T_SUBSCRIBE) {
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
          if (DLNADeviceMgr::instance) {
            Str sid = DLNADeviceMgr::instance->subscriptionMgr.subscribe(
                svcKey, cbStr.c_str(), tsec);
            server->replyHeader().setValues(200, "OK");
            server->replyHeader().put("SID", sid.c_str());
            server->replyHeader().put("TIMEOUT", "Second-1800");
            server->replyHeader().write(server->client());
            server->endClient();
            return;
          }
          server->replyNotFound();
          return;
        }
        // UNSUBSCRIBE: handle via SID header
        if (req.method() == T_POST) {
          // Some stacks use POST for unsubscribe; try to handle SID
          const char* sid = req.get("SID");
          if (sid && DLNADeviceMgr::instance) {
            bool ok = DLNADeviceMgr::instance->subscriptionMgr.unsubscribe(
                requestPath, sid);
            if (ok) {
              server->replyOK();
              return;
            }
          }
        }
        // default reply OK
        server->replyOK();
      };

      p_server->on(service.event_sub_url, T_GET, eventHandler, ref, 1);
    }

    return true;
  }

  /// callback to provide device XML
  static void deviceXMLCallback(HttpServer* server, const char* requestPath,
                                HttpRequestHandlerLine* hl) {
    DLNADevice* device_xml = (DLNADevice*)(hl->context[0]);
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