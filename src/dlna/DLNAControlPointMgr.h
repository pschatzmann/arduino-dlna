#pragma once

#include <functional>

#include "DLNAControlPointRequestParser.h"
#include "DLNADeviceInfo.h"
#include "DLNADevice.h"
#include "Schedule.h"
#include "Scheduler.h"
#include "basic/StrPrint.h"
#include "basic/Url.h"
#include "http/HttpServer.h"
#include "xml/XMLDeviceParser.h"
#include "xml/XMLParser.h"

namespace tiny_dlna {

class DLNAControlPointMgr;
DLNAControlPointMgr* selfDLNAControlPoint = nullptr;

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
class DLNAControlPointMgr {
 public:
  /// Default constructor w/o Notifications
  DLNAControlPointMgr() { selfDLNAControlPoint = this; }

  /// Constructor supporting Notifications
  DLNAControlPointMgr(HttpServer& server, int port = 80)
      : DLNAControlPointMgr() {
    setHttpServer(server, port);
  }
  /// Requests the parsing of the device information
  void setParseDevice(bool flag) { is_parse_device = flag; }

  /// Defines the lacal url (needed for subscriptions)
  void setLocalURL(Url url) { local_url = url; }

  /**
   * @brief Start discovery by sending M-SEARCH requests and process replies.
   *
   * searchTarget: the SSDP search target (e.g. "ssdp:all", device/service urn)
   * minWaitMs: minimum time in milliseconds to wait before returning a result
   *             (default: 3000 ms)
   * maxWaitMs: maximum time in milliseconds to wait for replies (M-SEARCH window)
   *             (default: 60000 ms)
   * Behavior: the function will wait at least `minWaitMs` and at most
   * `maxWaitMs`. If a device is discovered after `minWaitMs` elapsed the
   * function will return early; otherwise it will block until `maxWaitMs`.
   */
    bool begin(DLNAHttpRequest& http, IUDPService& udp,
               const char* searchTarget = "ssdp:all", uint32_t minWaitMs = 3000,
               uint32_t maxWaitMs = 60000) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNADevice::begin");
    search_target = searchTarget;
    is_active = true;
    p_udp = &udp;
    p_http = &http;

    if (p_http_server && http_server_port > 0 && eventCallback != nullptr) {
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
    search->repeat_ms = 1000;
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

    DlnaLogger.log(DlnaLogLevel::Info,
                   "Control Point started with %d devices found",
                   devices.size());
    return devices.size() > 0;
  }

  /// Stops the processing and releases the resources
  void end() {
    // p_server->end();
    for (auto& device : devices) device.clear();
    is_active = false;
    if (p_http_server) {
      p_http_server->end();
    }
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

  /// Subscribe to changes for all device services
  bool subscribeNotifications(DLNADeviceInfo& device, int timeoutSeconds = 60) {
    if (p_http_server==nullptr){
      DlnaLogger.log(DlnaLogLevel::Error, "HttpServer not defined - cannot subscribe to notifications");
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
  bool subscribeNotifications(DLNAServiceInfo& service, int timoutSeconds = 60) {
    if (p_http_server==nullptr){
      DlnaLogger.log(DlnaLogLevel::Error, "HttpServer not defined - cannot subscribe to notifications");
      return false;
    }
    DLNADeviceInfo& device = getDevice(service);

    char url_buffer[200] = {0};
    char seconds_txt[80] = {0};
    Url url{getUrl(device, service.event_sub_url, url_buffer, 200)};
    snprintf(seconds_txt, 80, "Second-%d", timoutSeconds);
    p_http->request().put("NT", "upnp:event");
    p_http->request().put("TIMEOUT", seconds_txt);
    p_http->request().put("CALLBACK", local_url.url());
    int rc = p_http->subscribe(url);
    DlnaLogger.log(DlnaLogLevel::Info, "Http rc: %s", rc);
    return rc == 200;
  }

  /// Register a callback that will be invoked for incoming event notification
  void onNotification(
      std::function<void(void* reference, const char* sid, const char* varName,
                         const char* newValue)>
          cb) {
    eventCallback = cb;
  }

  /// Attach an opaque reference pointer (optional, for caller context)
  void setReference(void* ref) { reference = ref; }

  /// Set HttpServer instance and register the notify handler
  void setHttpServer(HttpServer& server, int port = 80) {
    p_http_server = &server;
    http_server_port = port;
    attachHttpServer(server);
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

    if (p_http_server) {
      // handle server requests
      bool rc = p_http_server->doLoop();
    }

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

  /// Provides the device for a service
  DLNADeviceInfo& getDevice(DLNAServiceInfo& service) {
    for (auto& dev : devices) {
      for (auto& srv : dev.getServices()) {
        if (srv == srv) return dev;
      }
    }
    return NO_DEVICE;
  }

  /// Get a device for a Url
  DLNADeviceInfo& getDevice(Url location) {
    for (auto& dev : devices) {
      if (dev.getDeviceURL() == location) {
        return dev;
      }
    }
    return NO_DEVICE;
  }

  Vector<DLNADeviceInfo>& getDevices() { return devices; }

  /// Adds a new device
  bool addDevice(DLNADeviceInfo dev) {
    dev.updateTimestamp();
    for (auto& existing_device : devices) {
      if (dev.getUDN() == existing_device.getUDN()) {
        DlnaLogger.log(DlnaLogLevel::Info, "Device '%s' already exists",
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
    DLNADeviceInfo& device = getDevice(url);
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
      DlnaLogger.log(DlnaLogLevel::Error, "Http get to '%s' failed with %d",
                     url.url(), rc);
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
    DLNADeviceInfo new_device;
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
  Vector<DLNADeviceInfo> devices;
  Vector<ActionRequest> actions;
  XMLPrinter xml;
  bool is_active = false;
  bool is_parse_device = false;
  DLNADeviceInfo NO_DEVICE{false};
  const char* search_target;
  StringRegistry strings;
  Url local_url;
  HttpServer* p_http_server = nullptr;
  int http_server_port = 0;
  void* reference = nullptr;
  std::function<void(void* reference, const char* sid, const char* varName,
                     const char* newValue)> eventCallback;

  /// Attach an HttpServer so the control point can receive HTTP NOTIFY event
  /// messages. This registers a handler at the configured local URL path
  /// (see `local_url`) which will extract the SID header and body and call
  /// parseAndDispatchEvent() to dispatch property changes.
  void attachHttpServer(HttpServer& server) {
    p_http_server = &server;
    // register handler at the local path. If local_url is not set we use
    // a default path "/dlna/events"
    const char* path =
        StrView(local_url.url()).isEmpty() ? "/dlna/events" : local_url.path();

    // handler lambda: reads SID and body, forwards to parseAndDispatchEvent
    auto notifyHandler = [](HttpServer* server, const char* requestPath,
                            HttpRequestHandlerLine* hl) {
      // The DLNAControlPointMgr instance is passed as context[0]
      DLNAControlPointMgr* cp = nullptr;
      if (hl->contextCount > 0)
        cp = static_cast<DLNAControlPointMgr*>(hl->context[0]);
      if (!cp) {
        server->replyNotFound();
        return;
      }

      // read headers
      HttpRequestHeader& req = server->requestHeader();
      const char* sid = req.get("SID");

      // read body
      Str body = server->contentStr();

      // build temporary NotifyReplyCP and forward
      NotifyReplyCP tmp;
      if (sid) tmp.subscription_id = sid;
      tmp.xml = body.c_str();

      cp->parseAndDispatchEvent(tmp);

      server->replyOK();
    };

    void* ctx[1];
    ctx[0] = this;
    server.on(path, T_POST, notifyHandler, ctx, 1);
  }


  /// Processes a NotifyReplyCP message
  static bool processDevice(NotifyReplyCP& data) {
    Str& nts = data.nts;
    if (nts.equals("ssdp:byebye")) {
      selfDLNAControlPoint->processBye(nts);
      return true;
    }
    if (nts.equals("ssdp:alive")) {
      bool select = selfDLNAControlPoint->matches(data.usn.c_str());
      DlnaLogger.log(DlnaLogLevel::Info, "addDevice: %s -> %s",
                     data.usn.c_str(), select ? "added" : "filtered");
      Url url{data.location.c_str()};
      selfDLNAControlPoint->addDevice(url);
      return true;
    }
    return false;
  }

  // Parse the xml content of a NotifyReplyCP and dispatch each property
  void parseAndDispatchEvent(NotifyReplyCP& data) {
    // data.xml contains the <e:propertyset ...>...</e:propertyset> as a
    // string
    const char* xmlBuf = data.xml.c_str();
    if (xmlBuf == nullptr || *xmlBuf == '\0') return;

    struct CBRef {
      DLNAControlPointMgr* self;
      NotifyReplyCP* data;
    } ref;
    ref.self = this;
    ref.data = &data;

    // Simple callback: whenever the parser reports a text node (non-empty
    // `text`), treat `nodeName` as the variable name and `text` as its value.
    auto cb = [](Str& nodeName, Vector<Str>& /*path*/, Str& text, Str& /*attributes*/, int /*start*/,
                 int /*len*/, void* vref) {
      CBRef* r = static_cast<CBRef*>(vref);
      if (text.length() > 0 && r->self && r->self->eventCallback) {
        const char* sid = r->data->subscription_id.c_str();
        // store stable copies in the control point's string registry
        const char* namePtr = r->self->strings.add((char*)nodeName.c_str());
        const char* valPtr = r->self->strings.add((char*)text.c_str());
        // pass stored opaque reference as first parameter
        r->self->eventCallback(r->self->reference, sid, namePtr, valPtr);
      }
    };

    XMLParser parser(xmlBuf, cb);
    parser.setReference(&ref);
    parser.parse();
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
    DLNADeviceInfo& device = getDevice(service);

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

          // Extract payload substring and parse with XMLParser to get child elements
          Str payload;
          payload.copyFrom(soap.c_str() + payloadStart, payloadEnd - payloadStart);

          struct CBRef {
            DLNAControlPointMgr* self;
            ActionReply* out;
          } cbref;
          cbref.self = this;
          cbref.out = &result;

          auto xmlcb = [](Str& nodeName, Vector<Str>& /*path*/, Str& text, Str& /*attributes*/,
                          int /*start*/, int /*len*/, void* vref) {
            CBRef* r = static_cast<CBRef*>(vref);
            // trim text and ignore empty nodes
            Str value = text;
            value.trim();
            if (value.length() == 0) return;

            // strip namespace prefix from node name if present
            const char* fullName = nodeName.c_str();
            const char* sep = strchr(fullName, ':');
            const char* nameOnly = sep ? sep + 1 : fullName;

            const char* namePtr = r->self->strings.add((char*)nameOnly);
            const char* valuePtr = r->self->strings.add((char*)value.c_str());

            Argument arg;
            arg.name = namePtr;
            arg.value = valuePtr;
            r->out->arguments.push_back(arg);

            // Also notify application about this name/value via eventCallback
            if (r->self && r->self->eventCallback) {
              // SID is not applicable here (action response), pass nullptr
              r->self->eventCallback(r->self->reference, "", namePtr, valuePtr);
            }
          };

          XMLParser parser(payload.c_str(), xmlcb);
          parser.setReference(&cbref);
          parser.parse();
        }
      }
    }

    return result;
  }

  const char* getUrl(DLNADeviceInfo& device, const char* suffix, const char* buffer,
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