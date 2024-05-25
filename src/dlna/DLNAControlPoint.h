#pragma once

#include "DLNAControlPointRequestParser.h"
#include "DLNADevice.h"
#include "DLNADeviceInfo.h"
#include "Schedule.h"
#include "Scheduler.h"
#include "basic/StrPrint.h"
#include "basic/Url.h"
#include "http/HttpServer.h"
#include "xml/XMLDeviceParser.h"

namespace tiny_dlna {

/**
 * @brief Setup of a Basic DLNA Control Point.
 * The control point
 * - send out an MSearch request
 * - processes the MSearch replys
 *   - parses the Device xml
 *   - parses the Service xml
 * - subscribes to events
 * - pocesses the events
 *
 * The control point can also execute Actions
 *
 * @author Phil Schatzmann
 */
class DLNAControlPoint {
 public:
  /// Requests the parsing of the device information
  void setParseDevice(bool flag) { is_parse_device = flag; }

  /**
   * @brief start the processing by sending out a MSearch. For the search target
   * you can use:
     - ssdp:all : to search all UPnP devices,
     - upnp:rootdevice: only root devices . Embedded devices will not respond
     - uuid:device-uuid: search a device by vendor supplied unique id
     - urn:schemas-upnp-org:device:deviceType- version: locates all devices of a
given type (as defined by working committee)
     - urn:schemas-upnp-org:service:serviceType-
version: locate service of a given type
   */

  bool begin(HttpRequest& http, IUDPService& udp,
             const char* searchTarget = "ssdp:all", uint32_t processingTime = 0,
             bool stopWhenFound = true) {
    DlnaLogger.log(DlnaInfo, "DLNADevice::begin");

    p_udp = &udp;
    p_http = &http;

    // setup UDP
    if (!p_udp->begin(DLNABroadcastAddress)) {
      DlnaLogger.log(DlnaError, "UDP begin failed");
      return false;
    }

    // Send MSearch request via UDP
    scheduler.add(new MSearchSchedule(DLNABroadcastAddress, searchTarget));

    // if processingTime > 0 we do some loop processing already here
    uint32_t end = millis() + processingTime;
    while (millis() < end) {
      if (stopWhenFound && devices.size() > 0) break;
      loop();
    }

    is_active = true;
    DlnaLogger.log(DlnaInfo,
                   "Control Point successfully started with %d devices found",
                   devices.size());
    return true;
  }

  /// Stops the processing and releases the resources
  void end() {
    // p_server->end();
    for (auto& device : devices) device.clear();
    is_active = false;
  }

  /// Registers a method that will be called
  ActionRequest& addAction(ActionRequest act) {
    actions.push_back(act);
    return actions[actions.size() - 1];
  }

  /// Executes all registered methods
  ActionReply executeActions() {
    ActionReply result = postAllActions();
    actions.clear();
    return result;
  }

  // /// Provides the actual state value
  // const char* getStateValue(const char* name) {
  //   for (auto& st : state) {
  //     if (st.name == name) {
  //       return st.value.c_str();
  //     }
  //   }
  //   return nullptr;
  // }

  /// call this method in the Arduino loop as often as possible: the processes
  /// all replys
  bool loop() {
    if (!is_active) return false;
    DLNAControlPointRequestParser parser;

    // process UDP requests
    RequestData req = p_udp->receive();
    if (req) {
      Schedule* schedule = parser.parse(req);
      if (schedule != nullptr) {
        scheduler.add(schedule);
      }
    }

    // execute scheduled udp replys
    scheduler.execute(*p_udp);

    // be nice, if we have other tasks
    delay(5);
    return true;
  }

  /// Provide addess to the service information
  DLNAServiceInfo& getService(const char* id) {
    static DLNAServiceInfo no_service(false);
    for (auto& dev : devices) {
      DLNAServiceInfo& result = dev.getService(id);
      if (result) return result;
    }
    return no_service;
  }

  /// Provides the device information by index
  DLNADeviceInfo& getDevice(int deviceIdx = 0) { return devices[deviceIdx]; }

  DLNADeviceInfo& getDevice(DLNAServiceInfo& service) {
    for (auto& dev : devices) {
      for (auto& srv : dev.getServices()) {
        if (srv == srv) return dev;
      }
    }
    return no_device;
  }

  DLNADeviceInfo& getDevice(Url location) {
    for (auto& dev : devices) {
      if (dev.getDeviceURL() == location) {
        return dev;
      }
    }
    return no_device;
  }

  /// Adds a new device
  void addDevice(DLNADeviceInfo dev) {
    dev.updateTimestamp();
    for (auto& existing_device : devices) {
      if (dev.getUDN() == existing_device.getUDN()) {
        DlnaLogger.log(DlnaInfo, "Device '%s' already exists", dev.getUDN());
        return;
      }
    }
    DlnaLogger.log(DlnaInfo, "Device '%s' has been added", dev.getUDN());
    devices.push_back(dev);
  }

  /// We can activate/deactivate the scheduler
  void setActive(bool flag) { is_active = flag; }

  /// Checks if the scheduler is active
  bool isActive() { return is_active; }

 protected:
  Scheduler scheduler;
  IUDPService* p_udp = nullptr;
  HttpRequest* p_http = nullptr;
  Vector<DLNADeviceInfo> devices;
  // Vector<StateValue> state;
  Vector<ActionRequest> actions;
  XMLPrinter xml;
  bool is_active = false;
  bool is_parse_device = false;
  DLNADeviceInfo no_device{false};

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
    size_t result = xml.printXMLHeader();

    result += xml.printNodeBegin(
        "Envelope",
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"",
        "s");
    result += xml.printNodeBegin("Body", nullptr, "s");

    char ns[200];
    StrView namespace_str(ns, 200);
    namespace_str = "xmlns:u=\"%1\"";
    bool ok = namespace_str.replace("%1", action.getServiceType());
    DlnaLogger.log(DlnaInfo, "ns = '%s'", namespace_str.c_str());

    assert(ok);
    result += xml.printNodeBegin(action.action, namespace_str.c_str(), "u");
    for (auto arg : action.arguments) {
      result += xml.printNode(arg.name, arg.value.c_str());
    }
    result += xml.printNodeEnd(action.action, "u");

    result += xml.printNodeEnd("Body", "s");
    result += xml.printNodeEnd("Envelope", "s");
    return result;
  }

  ActionReply postAllActions() {
    ActionReply result;
    for (auto& action : actions) {
      result.add(postAction(action));
    }
    return result;
  }

  ActionReply postAction(ActionRequest& action) {
    DLNAServiceInfo& service = *action.p_service;
    DLNADeviceInfo& device = getDevice(service);

    // create XML
    StrPrint str_print;
    xml.setOutput(str_print);
    createXML(action);
    // log xml request
    DlnaLogger.log(DlnaInfo, str_print.c_str());

    // create SOAPACTION header
    char act[200];
    StrView action_str{act, 200};
    action_str = "\"";
    action_str.add(action.getServiceType());
    action_str.add("#");
    action_str.add(action.action);
    action_str.add("\"");

    p_http->request().put("SOAPACTION", action_str.c_str());

    // crate control url 
    char url_buffer[200];
    StrView url_str{url_buffer, 200};
    url_str = device.getBaseURL().url();
    url_str.add(service.control_url);
    Url post_url{url_str.c_str()};

    // post the request
    int rc = p_http->post(post_url, "text/xml", str_print.c_str(), str_print.length());
    DlnaLogger.log(DlnaInfo, "==> %d", rc);

    // receive result
    str_print.reset();
    uint8_t buffer[200];
    while(p_http->client()->available()){
      int len = p_http->client()->read(buffer, 200);
      str_print.write(buffer, len);
    } 

    // log result
    DlnaLogger.log(DlnaInfo, str_print.c_str());
    p_http->stop();

    ActionReply result(rc == 200);
    return result;
  }

};

}  // namespace tiny_dlna