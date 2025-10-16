#pragma once

#include "DLNAControlPointRequestParser.h"
#include "DLNADevice.h"
#include "DLNADeviceMgr.h"
#include "Schedule.h"
#include "Scheduler.h"
#include "basic/StrPrint.h"
#include "basic/Url.h"
#include "http/HttpServer.h"
#include "xml/XMLDeviceParser.h"

namespace tiny_dlna {

class DLNAControlPointMgr;
DLNAControlPointMgr* selfDLNAControlPoint = nullptr;

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
class DLNAControlPointMgr {
 public:
  DLNAControlPointMgr() { selfDLNAControlPoint = this; }
  /// Requests the parsing of the device information
  void setParseDevice(bool flag) { is_parse_device = flag; }
  /// Defines the lacal url (needed for subscriptions)
  void setLocalURL(Url url) { local_url = url; }

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

  bool begin(DLNAHttpRequest& http, IUDPService &udp,
             const char* searchTarget = "ssdp:all", uint32_t processingTime = 0,
             bool stopWhenFound = true) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::begin");
    search_target = searchTarget;
    is_active = true;
    p_udp = &udp;
    p_http = &http;

    // setup multicast UDP
    if (!(p_udp->begin(DLNABroadcastAddress))) {
      DlnaLogger.log(DlnaLogLevel::Error, "UDP begin failed");
      return false;
    }

    // Send MSearch request via UDP
    MSearchSchedule* search =
        new MSearchSchedule(DLNABroadcastAddress, searchTarget);
    search->end_time = millis() + 3000;
    search->repeat_ms = 1000;
    scheduler.add(search);

    // if processingTime > 0 we do some loop processing already here
    uint64_t end = millis() + processingTime;
    while (millis() < end) {
      if (stopWhenFound && devices.size() > 0) break;
      loop();
    }

    DlnaLogger.log(DlnaLogLevel::Info, "Control Point started with %d devices found",
                   devices.size());
    return devices.size() > 0;
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

  /// Subscribe to changes
  bool subscribe(const char* serviceName, int seconds) {
    auto service = getService(serviceName);
    if (!service) {
      DlnaLogger.log(DlnaLogLevel::Error, "No service found for %s", serviceName);
      return false;
    }

    auto& device = getDevice(service);
    if (!device) {
      DlnaLogger.log(DlnaLogLevel::Error, "Device not found");
      return false;
    }

    if (StrView(local_url.url()).isEmpty()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Local URL not defined");
      return false;
    }
    char url_buffer[200] = {0};
    char seconds_txt[80] = {0};
    Url url{getUrl(device, service.event_sub_url, url_buffer, 200)};
    snprintf(seconds_txt, 80, "Second-%d", seconds);
    p_http->request().put("NT", "upnp:event");
    p_http->request().put("TIMEOUT", seconds_txt);
    p_http->request().put("CALLBACK", local_url.url());
    int rc = p_http->subscribe(url);
    DlnaLogger.log(DlnaLogLevel::Info, "Http rc: %s", rc);
    return rc == 200;
  }

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
        // handle NotifyReplyCP
        if (StrView(schedule->name()).equals("NotifyReplyCP")) {
          NotifyReplyCP& notify_schedule = *(NotifyReplyCP*)schedule;
          notify_schedule.callback = processDevice;
        }
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
  DLNADevice& getDevice(int deviceIdx = 0) { return devices[deviceIdx]; }

  /// Provides the device for a service
  DLNADevice& getDevice(DLNAServiceInfo& service) {
    for (auto& dev : devices) {
      for (auto& srv : dev.getServices()) {
        if (srv == srv) return dev;
      }
    }
    return NO_DEVICE;
  }

  /// Get a device for a Url
  DLNADevice& getDevice(Url location) {
    for (auto& dev : devices) {
      if (dev.getDeviceURL() == location) {
        return dev;
      }
    }
    return NO_DEVICE;
  }

  Vector<DLNADevice>& getDevices() { return devices; }

  /// Adds a new device
  bool addDevice(DLNADevice dev) {
    dev.updateTimestamp();
    for (auto& existing_device : devices) {
      if (dev.getUDN() == existing_device.getUDN()) {
        DlnaLogger.log(DlnaLogLevel::Info, "Device '%s' already exists", dev.getUDN());
        return false;
      }
    }
    DlnaLogger.log(DlnaLogLevel::Info, "Device '%s' has been added", dev.getUDN());
    devices.push_back(dev);
    return true;
  }

  /// Adds the device from the device xml url if it does not already exist
  bool addDevice(Url url) {
    DLNADevice& device = getDevice(url);
    if (device != NO_DEVICE) {
      // device already exists
      device.setActive(true);
      return true;
    }
    // http get
    StrPrint xml;
    DLNAHttpRequest req;
    int rc = req.get(url, "text/xml");

    if (rc != 200) {
      DlnaLogger.log(DlnaLogLevel::Error, "Http get to '%s' failed with %d", url.url(),
                     rc);
      req.stop();
      return false;
    }
    // get xml
    uint8_t buffer[512];
    while (true) {
      int len = req.read(buffer, 512);
      if (len == 0) break;
      xml.write(buffer, len);
    }
    req.stop();

    // parse xml
    DLNADevice new_device;
    XMLDeviceParser parser;
    parser.parse(new_device, strings, xml.c_str());
    new_device.device_url = url;
    devices.push_back(new_device);
    return true;
  }

  /// We can activate/deactivate the scheduler
  void setActive(bool flag) { is_active = flag; }

  /// Checks if the scheduler is active
  bool isActive() { return is_active; }

 protected:
  Scheduler scheduler;
  DLNAHttpRequest* p_http = nullptr;
  IUDPService* p_udp = nullptr;
  Vector<DLNADevice> devices;
  Vector<ActionRequest> actions;
  XMLPrinter xml;
  bool is_active = false;
  bool is_parse_device = false;
  DLNADevice NO_DEVICE{false};
  const char* search_target;
  StringRegistry strings;
  Url local_url;

  /// Processes a NotifyReplyCP message
  static bool processDevice(NotifyReplyCP& data) {
    Str& nts = data.nts;
    if (nts.equals("ssdp:byebye")) {
      selfDLNAControlPoint->processBye(nts);
      return true;
    }
    if (nts.equals("ssdp:alive")) {
      bool select = selfDLNAControlPoint->matches(data.usn.c_str());
      DlnaLogger.log(DlnaLogLevel::Info, "addDevice: %s -> %s", data.usn.c_str(),
                     select ? "added" : "filtered");
      Url url{data.location.c_str()};
      selfDLNAControlPoint->addDevice(url);
      return true;
    }
    return false;
  }

  /// checks if the usn contains the search target
  bool matches(const char* usn) {
    if (StrView(search_target).equals("ssdp:all")) return true;
    return StrView(usn).contains(search_target);
  }

  /// processes a bye-bye message
  bool processBye(Str& usn) {
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
    DlnaLogger.log(DlnaLogLevel::Debug, "ns = '%s'", namespace_str.c_str());

    // assert(ok);
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
      if (action.getServiceType() != nullptr) result.add(postAction(action));
    }
    return result;
  }

  ActionReply postAction(ActionRequest& action) {
    DLNAServiceInfo& service = *action.p_service;
    DLNADevice& device = getDevice(service);

    // create XML
    StrPrint str_print;
    xml.setOutput(str_print);
    createXML(action);

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
    char url_buffer[200] = {0};
    Url post_url{getUrl(device, service.control_url, url_buffer, 200)};

    // post the request
    int rc = p_http->post(post_url, "text/xml", str_print.c_str(),
                          str_print.length());

    // check result
    DlnaLogger.log(DlnaLogLevel::Info, "==> http rc %d", rc);
    ActionReply result(rc == 200);
    if (rc != 200) {
      p_http->stop();
      return result;
    }

    // log xml request
    DlnaLogger.log(DlnaLogLevel::Debug, str_print.c_str());

    // receive result
    str_print.reset();
    uint8_t buffer[200];
    while (p_http->client()->available()) {
      int len = p_http->client()->read(buffer, 200);
      str_print.write(buffer, len);
    }

    // log result
    DlnaLogger.log(DlnaLogLevel::Debug, str_print.c_str());
    p_http->stop();

    // Try to parse SOAP response body and extract return arguments
    // e.g. <u:SomeActionResponse> <TrackDuration>01:23:45</TrackDuration> ...</u:SomeActionResponse>
    const char* resp = str_print.c_str();
    if (resp != nullptr && *resp != '\0') {
      StrView soap(resp);

      // Build response tag name: ActionNameResponse
      char respTagBuf[200] = {0};
      snprintf(respTagBuf, sizeof(respTagBuf), "%sResponse", action.action);
      int respPos = soap.indexOf(respTagBuf);
      if (respPos >= 0) {
        // find opening '<' before respPos
        int openPos = respPos;
        while (openPos > 0 && soap[openPos] != '<') openPos--;
        if (openPos < 0) openPos = 0;
        int openEnd = soap.indexOf('>', openPos);
        if (openEnd >= 0) {
          int payloadStart = openEnd + 1;
          // find end tag
          char endTagBuf[220] = {0};
          snprintf(endTagBuf, sizeof(endTagBuf), "</%sResponse>", action.action);
          int payloadEnd = soap.indexOf(endTagBuf, payloadStart);
          if (payloadEnd < 0) payloadEnd = soap.length();

          int pos = payloadStart;
          // iterate over child elements inside the response
          while (true) {
            int lt = soap.indexOf('<', pos);
            if (lt < 0 || lt >= payloadEnd) break;
            // skip closing tags
            if (lt + 1 < soap.length() && soap[lt + 1] == '/') {
              pos = lt + 2;
              continue;
            }
            int nameStart = lt + 1;
            // read element name until space or '>'
            int nameEnd = nameStart;
            while (nameEnd < soap.length()) {
              char c = soap[nameEnd];
              if (c == '>' || c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
              nameEnd++;
            }
            if (nameEnd <= nameStart) break;

            // extract name and value
            Str nameStr;
            nameStr.copyFrom(soap.c_str() + nameStart, nameEnd - nameStart);

            int valueStart = soap.indexOf('>', lt);
            if (valueStart < 0) break;
            valueStart += 1;
            // closing tag for this element
            char closeTag[220] = {0};
            // use name without any namespace prefix
            const char* fullName = nameStr.c_str();
            const char* sep = strchr(fullName, ':');
            const char* nameOnly = fullName;
            if (sep) nameOnly = sep + 1;
            snprintf(closeTag, sizeof(closeTag), "</%s>", nameOnly);
            int closePos = soap.indexOf(closeTag, valueStart);
            if (closePos < 0 || closePos > payloadEnd) break;

            Str valueStr;
            valueStr.copyFrom(soap.c_str() + valueStart, closePos - valueStart);

            // trim whitespace
            valueStr.trim();

            // store in string registry so pointers remain valid
            const char* namePtr = strings.add((char*)nameOnly);
            const char* valuePtr = strings.add((char*)valueStr.c_str());

            Argument arg;
            arg.name = namePtr;
            arg.value = valuePtr;
            result.arguments.push_back(arg);

            pos = closePos + strlen(closeTag);
          }
        }
      }
    }

    return result;
  }

  const char* getUrl(DLNADevice& device, const char* suffix, const char* buffer,
                     int len) {
    StrView url_str{(char*)buffer, len};
    url_str = device.getBaseURL();
    if (url_str == nullptr) {
      url_str.add(device.getDeviceURL().protocol());
      url_str.add("://");
      url_str.add(device.getDeviceURL().host());
      url_str.add(":");
      url_str.add(device.getDeviceURL().port());
    }
    url_str.add(suffix);
    return buffer;
  }
};

}  // namespace tiny_dlna