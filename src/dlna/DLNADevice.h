#pragma once

#include "DLNADevice.h"
#include "DLNADeviceInfo.h"
#include "DLNARequestParser.h"
#include "Schedule.h"
#include "basic/Url.h"
#include "http/HttpServer.h"

#define DLNA_MAX_URL_LEN 120

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
class DLNADevice {
 public:
  /// start the
  bool begin(DLNADeviceInfo& device, IUDPService& udp, HttpServer& server) {
    DlnaLogger.log(DlnaInfo, "DLNADevice::begin");

    p_server = &server;
    p_udp = &udp;
    addDevice(device);
    setupParser();

    // check base url
    Url baseUrl = device.getBaseURL();
    DlnaLogger.log(DlnaInfo, "base URL: %s", baseUrl.url());

    if (StrView(device.getBaseURL().host()).equals("localhost")) {
      DlnaLogger.log(DlnaError, "invalid base address: %s", baseUrl.url());
      return false;
    }

    // setup all services
    for (auto& p_device : devices) setupServices(*p_device);

    // setup web server
    if (!setupDLNAServer(server)) {
      DlnaLogger.log(DlnaError, "setupDLNAServer failed");
      return false;
    }

    // start web server
    if (!p_server->begin(baseUrl.port())) {
      DlnaLogger.log(DlnaError, "Server failed");
      return false;
    }

    // setup UDP
    if (!p_udp->begin(DLNABroadcastAddress)) {
      DlnaLogger.log(DlnaError, "UDP begin failed");
      return false;
    }

    if (!setupScheduler()) {
      DlnaLogger.log(DlnaError, "Scheduler failed");
      return false;
    }

    is_active = true;
    DlnaLogger.log(DlnaInfo, "Device successfully started");
    return true;
  }

  /// We potentially support some additional devices on top of the one defined
  /// via begin
  void addDevice(DLNADeviceInfo& device) { devices.push_back(&device); }

  /// Stops the processing and releases the resources
  void end() {
    p_server->end();

    // send 3 bye messages
    PostByeSchedule* bye = new PostByeSchedule();
    bye->repeat_ms = 800;
    scheduler.add(bye);

    // execute scheduler for 2 seconds
    uint32_t end = millis() + 2000;
    while (millis() < end) {
      scheduler.execute(*p_udp, getDevice());
    }

    for (auto& p_device : devices) p_device->clear();

    is_active = false;
  }

  /// call this method in the Arduino loop as often as possible
  bool loop() {
    if (!is_active) return false;

    // handle server requests
    bool rc = p_server->doLoop();
    DlnaLogger.log(DlnaDebug, "server %s", rc ? "true" : "false");

    if (isSchedulerActive()) {
      // process UDP requests
      RequestData req = p_udp->receive();
      if (req) {
        Schedule* schedule = parser.parse(req);
        if (schedule != nullptr) {
          scheduler.add(schedule);
        }
      }

      // execute scheduled udp replys
      scheduler.execute(*p_udp, getDevice());
    }

    // be nice, if we have other tasks
    delay(5);

    return true;
  }

  /// Provide addess to the service information
  DLNAServiceInfo getService(const char* id, int deviceIdx = 0) {
    return devices[deviceIdx]->getService(id);
  }

  /// Provides the device
  DLNADeviceInfo& getDevice(int deviceIdx = 0) { return *devices[deviceIdx]; }

  /// We can activate/deactivate the scheduler
  void setSchedulerActive(bool flag) { scheduler_active = flag; }

  /// Checks if the scheduler is active
  bool isSchedulerActive() { return scheduler_active; }

  /// Repeat the post-alive messages (default: 0 = no repeat). Call this method
  /// before calling begin!
  void setPostAliveRepeatMs(uint32_t ms) { post_alive_repeat_ms = ms; }

 protected:
  Scheduler scheduler;
  DLNARequestParser parser;
  IUDPService* p_udp = nullptr;
  Vector<DLNADeviceInfo*> devices;
  HttpServer* p_server = nullptr;
  bool is_active = false;
  bool scheduler_active = true;
  uint32_t post_alive_repeat_ms = 0;

  /// MSearch requests reply to upnp:rootdevice and the device type defined in
  /// the device
  bool setupParser() {
    parser.addMSearchST("upnp:rootdevice");
    parser.addMSearchST("ssdp:all");
    for (auto& device : devices) {
      parser.addMSearchST(device->getUDN());
      parser.addMSearchST(device->getDeviceType());
    }
    return true;
  }

  /// Schedule PostAlive messages
  bool setupScheduler() {
    // schedule post alive messages: Usually repeated 2 times (because UDP
    // messages might be lost)
    PostAliveSchedule* postAlive = new PostAliveSchedule(post_alive_repeat_ms);
    PostAliveSchedule* postAlive1 = new PostAliveSchedule(post_alive_repeat_ms);
    postAlive1->time = millis() + 100;
    scheduler.add(postAlive);
    scheduler.add(postAlive1);
    return true;
  }

  /// set up Web Server to handle Service Addresses
  virtual bool setupDLNAServer(HttpServer& srv) {
    // make sure we have any devices
    if (devices.empty()) {
      DlnaLogger.log(DlnaError, "No devices found");
      return false;
    }

    // Setup services for all devices
    for (auto& p_device : devices) {
      // add device url to server
      const char* device_path = p_device->getDeviceURL().path();
      const char* prefix = p_device->getBaseURL().path();

      DlnaLogger.log(DlnaInfo, "Setting up device path: %s", device_path);
      void* ref[] = {p_device};

      if (!StrView(device_path).isEmpty()) {
        p_server->rewrite("/", device_path);
        p_server->rewrite("/index.html", device_path);
        p_server->on(device_path, T_GET, deviceXMLCallback, ref, 1);
      }

      // Register icon and privide favicon.ico
      Icon icon = p_device->getIcon();
      if (icon.icon_data != nullptr) {
        char tmp[DLNA_MAX_URL_LEN];
        const char* icon_path =
            concat(prefix, icon.icon_url, tmp, DLNA_MAX_URL_LEN);
        p_server->on(icon_path, T_GET, icon.mime,
                     (const uint8_t*)icon.icon_data, icon.icon_size);
        p_server->on("/favicon.ico", T_GET, icon.mime,
                     (const uint8_t*)icon.icon_data, icon.icon_size);
      }

      // Register Service URLs
      char tmp[DLNA_MAX_URL_LEN];
      for (DLNAServiceInfo& service : p_device->getServices()) {
        p_server->on(concat(prefix, service.scp_url, tmp, DLNA_MAX_URL_LEN),
                     T_GET, service.scp_cb, ref, 1);
        p_server->on(concat(prefix, service.control_url, tmp, DLNA_MAX_URL_LEN),
                     T_POST, service.control_cb, ref, 1);
        p_server->on(
            concat(prefix, service.event_sub_url, tmp, DLNA_MAX_URL_LEN), T_GET,
            service.event_sub_cb, ref, 1);
      }
    }
    return true;
  }

  /// concatenate strings without allocating any heap memory
  const char* concat(const char* prefix, const char* addr, char* buffer,
                     int len) {
    StrView str(buffer, len);
    str = prefix;
    str += addr;
    str.replace("//", "/");
    return buffer;
  }

  /// callback to provide device XML
  static void deviceXMLCallback(HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
    DLNADeviceInfo* device_xml = (DLNADeviceInfo*)(hl->context[0]);
    assert(device_xml != nullptr);
    if (device_xml != nullptr) {
      Client& client = server->client();
      assert(&client != nullptr);
      DlnaLogger.log(DlnaInfo, "reply %s", "callback");
      server->replyHeader().setValues(200, "SUCCESS");
      server->replyHeader().put(CONTENT_TYPE, "text/xml");
      server->replyHeader().put(CONNECTION, CON_KEEP_ALIVE);
      server->replyHeader().write(client);

      // print xml result
      device_xml->print(client);
      server->endClient();
    } else {
      DlnaLogger.log(DlnaError, "DLNADeviceInfo is null");
      server->replyNotFound();
    }
  }


  /// If you dont already provid a complete DLNADeviceInfo you can overwrite
  /// this method and add some custom device specific logic to implement a new
  /// device. The MediaRenderer is using this approach!
  virtual void setupServices(DLNADeviceInfo& deviceInfo) {};
};

}  // namespace tiny_dlna