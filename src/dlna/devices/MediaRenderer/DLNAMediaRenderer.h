#pragma once
#include <cctype>
#include <cstring>

#include "DLNAMediaRendererDescr.h"
#include "basic/Str.h"
#include "dlna/devices/DLNADevice.h"
#include "dlna/xml/XMLAttributeParser.h"
#include "dlna/xml/XMLParserPrint.h"

namespace tiny_dlna {
/**
 * @enum MediaEvent
 * @brief Events emitted by the MediaRenderer to notify the application
 *        about playback and rendering control changes.
 *
 * The application registers a callback using setMediaEventHandler() and
 * will receive the event along with a reference to the emitting
 * MediaRenderer instance. The handler should query the renderer (for
 * example getCurrentUri(), getVolume(), getMime()) to determine the
 * appropriate action for each event.
 *
 * Events:
 * - SET_URI:    The renderer received a new URI (use getCurrentUri()).
 * - PLAY:       Start or resume playback of the current URI.
 * - PAUSE:      Pause playback.
 * - STOP:       Stop playback and reset playback state.
 * - SET_VOLUME: The volume was changed (use getVolume()).
 * - SET_MUTE:   The mute state changed (use isMuted()).
 */
enum class MediaEvent { SET_URI, PLAY, PAUSE, STOP, SET_VOLUME, SET_MUTE };

/**
 * @brief MediaRenderer DLNA Device
 *
 * MediaRenderer implements a simple UPnP/DLNA Media Renderer device. It
 * receives stream URIs via UPnP AVTransport actions and delegates actual
 * playback and rendering to the application through an event callback API.
 * This removes any dependency on an internal audio stack: applications
 * handle playback themselves by registering a handler with
 * setMediaEventHandler(). The device still supports rendering controls
 * (volume, mute) and transport controls (play/pause/stop) and provides
 * helper accessors like getCurrentUri() and getMime().
 *
 * Usage summary:
 * - Register an event handler with setMediaEventHandler() to receive
 *   MediaEvent notifications (SET_URI, PLAY, PAUSE, STOP, SET_VOLUME,
 *   SET_MUTE).
 * - In the handler query the renderer (getCurrentUri(), getVolume(),
 *   isMuted(), getMime()) and implement platform-specific playback.
 * - Use play(url) to programmatically start playback; the handler will
 *   also be notified of SET_URI and PLAY when the device receives a
 *   SetAVTransportURI SOAP action.
 *
 * This class is intentionally small and Arduino-friendly: methods return
 * bool for success/failure and avoid heavy dynamic memory allocations in
 * the hot path.
 *
 * Author: Phil Schatzmann
 */
class DLNAMediaRenderer : public DLNADeviceInfo {
 public:
  // event handler: (event, reference to MediaRenderer)
  typedef void (*MediaEventHandler)(MediaEvent event,
                                    DLNAMediaRenderer& renderer);

  /**
   * @brief Default constructor
   *
   * Initializes device metadata (friendly name, manufacturer, model) and
   * sets default base URL and identifiers. It does not configure any audio
   * pipeline components; use setOutput() and setDecoder() for that.
   */
  DLNAMediaRenderer() {
    // Constructor
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::MediaRenderer");
    setSerialNumber(usn);
    setDeviceType(st);
    setFriendlyName("ArduinoMediaRenderer");
    setManufacturer("TinyDLNA");
    setModelName("TinyDLNA");
    setModelNumber("1.0");
    setBaseURL("http://localhost:44757");
    setupRules();
  }

  /**
   *  @brief Recommended constructor
   * Construct a MediaRenderer bound to an HttpServer and IUDPService.
   * The provided references are stored and used when calling begin().
   */
  DLNAMediaRenderer(HttpServer& server, IUDPService& udp)
      : DLNAMediaRenderer() {
    setHttpServer(server);
    setUdpService(udp);
  }

  // Start the underlying DLNA device using the stored server/udp
  bool begin() { return dlna_device.begin(*this, *p_udp_member, *p_server); }

  /// Stops processing and releases resources
  void end() {
    if (is_active) stop();
    dlna_device.end();
  }

  /// Call this from Arduino loop()
  bool loop() { return dlna_device.loop(); }

  /// Set the http server instance the MediaRenderer should use
  void setHttpServer(HttpServer& server) {
    p_server = &server;
    // prepare handler context and register each service via helpers
    ref_ctx[0] = this;
    setupServicesImpl(&server);
  }

  /// Set the UDP service instance the MediaRenderer should use
  void setUdpService(IUDPService& udp) { p_udp_member = &udp; }

  /// Query whether renderer is active (playing)
  bool isActive() { return is_active; }

  /// Set the active state (used by transport callbacks)
  void setActive(bool active) {
    DlnaLogger.log(DlnaLogLevel::Info, "Set active: %s",
                   active ? "true" : "false");
    is_active = active;
    if (active) {
      start_time = millis();
    } else {
      // accumulate time
      time_sum += millis() - start_time;
      start_time = 0;
    }
    StrPrint cmd;
    cmd.printf("<TransportState val=\"%s\"/>",
               active ? "PLAYING" : "PAUSED_PLAYBACK");
    publishProperty("AVT", cmd.c_str());
  }

  /// Provides the mime from the DIDL or nullptr
  const char* getMime() {
    // Prefer explicit MIME from DIDL if available
    if (!current_mime.isEmpty()) return current_mime.c_str();
    return nullptr;
  }

  /// Register application event handler
  void setMediaEventHandler(MediaEventHandler cb) { event_cb = cb; }

  /// Get current volume (0-255)
  uint8_t getVolume() { return current_volume; }

  /// Set volume and publish event
  void setVolume(uint8_t vol) {
    current_volume = vol;
    StrPrint cmd;
    cmd.printf("<Volume val=\"%d\"/>", current_volume);
    publishProperty("RCS", cmd.c_str());
    if (event_cb) event_cb(MediaEvent::SET_VOLUME, *this);
  }

  /// Query mute state
  bool isMuted() { return is_muted; }

  /// Set mute state and publish event
  void setMuted(bool m) {
    is_muted = m;
    StrPrint cmd;
    cmd.printf("<Mute val=\"%d\"/>", is_muted ? 1 : 0);
    publishProperty("RCS", cmd.c_str());
    if (event_cb) event_cb(MediaEvent::SET_MUTE, *this);
  }

  // Access current URI
  const char* getCurrentUri() { return current_uri.c_str(); }

  /// Get textual transport state
  const char* getTransportState() { return transport_state.c_str(); }

  /// Provides access to the internal DLNA device instance
  DLNADevice& device() { return dlna_device; }

  /// Start playback: same as setActive(true)
  bool play() {
    setActive(true);
    return true;
  }

  /// Start playback of a network resource (returns true on success)
  bool play(const char* urlStr) {
    if (urlStr == nullptr) return false;
    DlnaLogger.log(DlnaLogLevel::Info, "play URL: %s", urlStr);
    // store URI
    current_uri.set(urlStr);
    // notify handler about the new URI and play
    if (event_cb) {
      event_cb(MediaEvent::SET_URI, *this);
      event_cb(MediaEvent::PLAY, *this);
    }
    is_active = true;
    start_time = millis();
    time_sum = 0;

    StrPrint cmd;
    cmd.printf("<CurrentTrackURI val=\"%s\"/>\n", current_uri.c_str());
    cmd.printf("<TransportState val=\"PLAYING\"/>");
    publishProperty("AVT", cmd.c_str());

    return true;
  }

  /// Defines the actual url to play
  bool setPlaybackURL(const char* urlStr) {
    if (urlStr == nullptr) return false;
    DlnaLogger.log(DlnaLogLevel::Info, "setPlaybackURL URL: %s", urlStr);
    // store URI
    current_uri = Str(urlStr);
    // notify handler about the new URI and play
    if (event_cb) {
      event_cb(MediaEvent::SET_URI, *this);
    }

    StrPrint cmd;
    cmd.printf("<CurrentTrackURI val=\"%s\"/>\n", current_uri.c_str());
    publishProperty("AVT", cmd.c_str());

    return true;
  }

  /// Stop playback
  bool stop() {
    DlnaLogger.log(DlnaLogLevel::Info, "Stop playback");
    is_active = false;
    start_time = 0;
    time_sum = 0;
    // notify handler about the stop
    if (event_cb) event_cb(MediaEvent::STOP, *this);
    StrPrint cmd;
    cmd.printf("<TransportState val=\"STOPPED\"/>");
    publishProperty("AVT", cmd.c_str());
    return true;
  }

  /**
   * @brief Notify the renderer that playback completed.
   *
   * This helper updates the internal transport state to "STOPPED",
   * marks the renderer inactive, resets the start time, logs the event and
   * notifies the application event handler with MediaEvent::STOP so the
   * application can perform cleanup (release resources, update UI, etc.).
   */
  void setPlaybackCompleted() {
    DlnaLogger.log(DlnaLogLevel::Info, "Playback completed");
    transport_state = "STOPPED";
    is_active = false;
    start_time = 0;
    time_sum = 0;
    // notify application handler about the stop
    if (event_cb) event_cb(MediaEvent::STOP, *this);
    // publish UPnP event to subscribers (if subscription manager available)
    StrPrint cmd;
    cmd.printf("<TransportState val=\"STOPPED\"/>");
    cmd.printf("<CurrentPlayMode val=\"NORMAL\"/>");
    cmd.printf("<CurrentTrackURI val=\"\"/>");
    cmd.printf("<RelativeTimePosition val=\"00:00:00\"/>");
    cmd.printf("<CurrentTransportActions val=\"Play\"/>");
    publishProperty("AVT", cmd.c_str());
  }

  /// Get estimated playback position (seconds)
  size_t getRelativeTimePositionSec() {
    unsigned long posMs = (millis() - start_time) + time_sum;
    return static_cast<size_t>(posMs / 1000);
  }

  /// Publish the RelativeTimePosition property
  void publishGetRelativeTimePositionSec() {
    StrPrint cmd;
    cmd.printf("<RelativeTimePosition val=\"%02d:%02d:%02d\"/>",
               static_cast<int>(getRelativeTimePositionSec() / 3600),
               static_cast<int>((getRelativeTimePositionSec() % 3600) / 60),
               static_cast<int>(getRelativeTimePositionSec() % 60));
    publishProperty("AVT", cmd.c_str());
  }

  /// Get a csv of the valid actions
  const char* getCurrentTransportActions() {
    if (is_active) {
      return "Pause,Stop";
    } else {
      return "Play,Stop";
    }
  }
  /// Defines a custom action rule for the media renderer.
  void setCustomActionRule(const char* suffix,
                           bool (*handler)(DLNAMediaRenderer*, ActionRequest&,
                                           HttpServer&)) {
    for (size_t i = 0; i < rules.size(); ++i) {
      if (StrView(rules[i].suffix).equals(suffix)) {
        rules[i].handler = handler;
        return;
      }
    }
    rules.push_back({suffix, handler});
  }

 protected:
  tiny_dlna::Str current_uri;
  tiny_dlna::Str current_mime;
  MediaEventHandler event_cb = nullptr;
  uint8_t current_volume = 50;
  bool is_muted = false;
  unsigned long start_time = 0;
  unsigned long time_sum = 0;
  // Current transport state string (e.g. "STOPPED", "PLAYING",
  // "PAUSED_PLAYBACK")
  tiny_dlna::Str transport_state = "NO_MEDIA_PRESENT";
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "uuid:09349455-2941-4cf7-9847-1dd5ab210e97";

  // internal DLNA device instance owned by this MediaRenderer
  DLNADevice dlna_device;
  HttpServer* p_server = nullptr;
  IUDPService* p_udp_member = nullptr;
  // reusable context pointer array for HttpServer handlers (contains `this`)
  void* ref_ctx[1] = {nullptr};
  Vector<ActionRule> rules;

  /// serviceAbbrev: AVT, RCS, CMS
  void publishProperty(const char* serviceAbbrev, const char* changeTag) {
    // Delegate to the internal DLNADevice instance which manages
    // subscriptions and services.
    dlna_device.publishProperty(serviceAbbrev, changeTag);
  }

  /// Publish the current AVTransport state (TransportState, CurrentTrackURI,
  /// RelativeTimePosition, CurrentTransportActions)
  void publishAVT() {
    StrPrint cmd;
    // Transport state
    cmd.printf("<TransportState val=\"%s\"/>", transport_state.c_str());
    // Current track URI, if any
    if (!current_uri.isEmpty()) {
      cmd.printf("<CurrentTrackURI val=\"%s\"/>", current_uri.c_str());
    }
    // Relative time position
    cmd.printf("<RelativeTimePosition val=\"%02d:%02d:%02d\"/>",
               static_cast<int>(getRelativeTimePositionSec() / 3600),
               static_cast<int>((getRelativeTimePositionSec() % 3600) / 60),
               static_cast<int>(getRelativeTimePositionSec() % 60));
    // Current transport actions
    cmd.printf("<CurrentTransportActions val=\"%s\"/>",
               getCurrentTransportActions());
    publishProperty("AVT", cmd.c_str());
  }

  /// Publish the current RenderingControl state (Volume, Mute)
  void publishRCS() {
    StrPrint cmd;
    cmd.printf("<Volume val=\"%d\"/>", current_volume);
    cmd.printf("<Mute val=\"%d\"/>", is_muted ? 1 : 0);
    publishProperty("RCS", cmd.c_str());
  }

  /// Publish a minimal ConnectionManager state (CurrentConnectionIDs)
  void publishCMS() {
    StrPrint cmd;
    // Minimal information: list of current connection IDs. Use "0" as default
    // when no active connection is present.
    cmd.printf("<CurrentConnectionIDs>0</CurrentConnectionIDs>");
    publishProperty("CMS", cmd.c_str());
  }

  /// Set MIME explicitly (used when DIDL-Lite metadata provides protocolInfo)
  void setMime(const char* mime) {
    if (mime) {
      current_mime = mime;
      DlnaLogger.log(DlnaLogLevel::Info, "Set mime: %s", current_mime.c_str());
    }
  }

  /// Try to parse a DIDL-Lite snippet and extract the protocolInfo MIME from
  /// the <res> element. If found, set current_mime.
  void setMimeFromDIDL(const char* didl) {
    if (!didl) return;
    // Convenience helper: extract protocolInfo attribute from a tag and
    // return the contentFormat (3rd token) portion, e.g. from
    // protocolInfo="http-get:*:audio/mpeg:*" -> "audio/mpeg".
    char mimeBuf[64] = {0};
    if (XMLAttributeParser::extractAttributeToken(
            didl, "<res", "protocolInfo=", 3, mimeBuf, sizeof(mimeBuf))) {
      setMime(mimeBuf);
    }
  }

  // Static descriptor callbacks: print the SCPD XML for each service
  static void transportDescrCB(HttpServer* server, const char* requestPath,
                               HttpRequestHandlerLine* hl) {
    server->reply("text/xml", [](Print& out) {
      DLNAMediaRendererTransportDescr d;
      d.printDescr(out);
    });
  }

  static void connmgrDescrCB(HttpServer* server, const char* requestPath,
                             HttpRequestHandlerLine* hl) {
    server->reply("text/xml", [](Print& out) {
      DLNAMediaRendererConnectionMgrDescr d;
      d.printDescr(out);
    });
  }

  static void controlDescrCB(HttpServer* server, const char* requestPath,
                             HttpRequestHandlerLine* hl) {
    server->reply("text/xml", [](Print& out) {
      DLNAMediaRendererControlDescr d;
      d.printDescr(out);
    });
  }

  // Generic control callback: parse SOAP action and dispatch to instance
  static void controlCB(HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DLNADevice::parseActionRequest(server, requestPath, hl, action);

    DLNAMediaRenderer* mr = nullptr;
    if (hl && hl->context[0]) mr = (DLNAMediaRenderer*)hl->context[0];

    if (mr) {
      if (mr->processAction(action, *server)) return;
    }

    server->replyNotFound();
  }

   /// After the subscription we publish all relevant properties
  static void eventSubscriptionHandler(HttpServer* server,
                                       const char* requestPath,
                                       HttpRequestHandlerLine* hl) {
    DLNADevice::eventSubscriptionHandler(server, requestPath, hl);
    DLNAMediaRenderer* device = (DLNAMediaRenderer*)(hl->context[0]);
    if (device) {
      StrView request_path_str(requestPath);
      if (request_path_str.contains("/AVT/")) device->publishAVT();
      else if (request_path_str.contains("/RC/")) device->publishRCS();
      else if (request_path_str.contains("/CM/")) device->publishCMS();
    }
  }
 
  // Setup and register individual services
  void setupTransportService(HttpServer* server) {
    DLNAServiceInfo avt;
    // Use static descriptor callback and control handler
    avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
              &DLNAMediaRenderer::transportDescrCB, "/AVT/control",
              &DLNAMediaRenderer::controlCB, "/AVT/event",
              [](HttpServer* server, const char* requestPath,
                 HttpRequestHandlerLine* hl) { server->replyOK(); });
    avt.subscription_namespace_abbrev = "AVT";
    // register URLs on the provided server so SCPD, control and event
    // subscription endpoints are available immediately
    server->on(avt.scpd_url, T_GET, avt.scp_cb, ref_ctx, 1);
    server->on(avt.control_url, T_POST, avt.control_cb, ref_ctx, 1);
    server->on(avt.event_sub_url, T_SUBSCRIBE, eventSubscriptionHandler,
               ref_ctx, 1);
    server->on(avt.event_sub_url, T_UNSUBSCRIBE,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);
    server->on(avt.event_sub_url, T_POST,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);

    addService(avt);
  }

  void setupConnectionManagerService(HttpServer* server) {
    DLNAServiceInfo cm;
    cm.setup(
        "urn:schemas-upnp-org:service:ConnectionManager:1",
        "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
        &DLNAMediaRenderer::connmgrDescrCB, "/CM/control",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); },
        "/CM/event",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); });
    cm.subscription_namespace_abbrev = "CMS";

    server->on(cm.scpd_url, T_GET, cm.scp_cb, ref_ctx, 1);
    server->on(cm.control_url, T_POST, cm.control_cb, ref_ctx, 1);
    server->on(cm.event_sub_url, T_SUBSCRIBE,
               eventSubscriptionHandler, ref_ctx, 1);
    server->on(cm.event_sub_url, T_UNSUBSCRIBE,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);
    server->on(cm.event_sub_url, T_POST,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);

    addService(cm);
  }

  void setupRenderingControlService(HttpServer* server) {
    DLNAServiceInfo rc;
    rc.setup("urn:schemas-upnp-org:service:RenderingControl:1",
             "urn:upnp-org:serviceId:RenderingControl", "/RC/service.xml",
             &DLNAMediaRenderer::controlDescrCB, "/RC/control",
             &DLNAMediaRenderer::controlCB, "/RC/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });
    rc.subscription_namespace_abbrev = "RCS";
    server->on(rc.scpd_url, T_GET, rc.scp_cb, ref_ctx, 1);
    server->on(rc.control_url, T_POST, rc.control_cb, ref_ctx, 1);
    server->on(rc.event_sub_url, T_SUBSCRIBE,
               eventSubscriptionHandler, ref_ctx, 1);
    server->on(rc.event_sub_url, T_UNSUBSCRIBE,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);
    server->on(rc.event_sub_url, T_POST,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);

    addService(rc);
  }

  /// Register all services (called when HTTP server is set)
  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::setupServices");
    // register the individual services and register their endpoints on the
    // provided HttpServer
    setupTransportService(server);
    setupConnectionManagerService(server);
    setupRenderingControlService(server);
  }

  /// Builds a standard SOAP reply envelope
  static int printReplyXML(Print& out, const char* replyName,
                           const char* serviceId,
                           const char* values = nullptr) {
    XMLPrinter xp(out);
    size_t result = 0;
    result += xp.printNodeBegin(
        "s:Envelope", "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"");
    result += xp.printNodeBegin("s:Body");
    result += xp.printf("<u:%s xmlns:u=\"urn:schemas-upnp-org:service:%s:1\">",
                        replyName, serviceId);

    // e.g.<u:return>Stop,Pause,Next,Seek</u:return> for
    // QueryStateVariableResponse
    if (values) {
      result += xp.printf("%s", values);
    }

    result += xp.printf("</u:%s>", replyName);
    result += xp.printNodeEnd("s:Body");
    result += xp.printNodeEnd("s:Envelope");
    return result;
  }

  // Static reply helper methods for SOAP actions
  static size_t replyPlay(Print& out) {
    return DLNAMediaRenderer::printReplyXML(out, "PlayResponse", "AVTransport");
  }
  static size_t replyPause(Print& out) {
    return DLNAMediaRenderer::printReplyXML(out, "PauseResponse",
                                            "AVTransport");
  }
  static size_t replyStop(Print& out) {
    return DLNAMediaRenderer::printReplyXML(out, "StopResponse", "AVTransport");
  }
  static size_t replySetAVTransportURI(Print& out) {
    return DLNAMediaRenderer::printReplyXML(out, "SetAVTransportURIResponse",
                                            "AVTransport");
  }
  static size_t replySetVolume(Print& out) {
    return DLNAMediaRenderer::printReplyXML(out, "SetVolumeResponse",
                                            "RenderingControl");
  }
  static size_t replySetMute(Print& out) {
    return DLNAMediaRenderer::printReplyXML(out, "SetMuteResponse",
                                            "RenderingControl");
  }
  // Static reply helper for GetMute
  static size_t replyGetMute(Print& out, bool isMuted) {
    StrPrint values;
    values.printf("<CurrentMute>%d</CurrentMute>", isMuted ? 1 : 0);
    return DLNAMediaRenderer::printReplyXML(out, "GetMuteResponse",
                                            "RenderingControl", values.c_str());
  }

  // Static reply helper for GetVolume
  static size_t replyGetVolume(Print& out, uint8_t volume) {
    StrPrint values;
    values.printf("<CurrentVolume>%d</CurrentVolume>", volume);
    return DLNAMediaRenderer::printReplyXML(out, "GetVolumeResponse",
                                            "RenderingControl", values.c_str());
  }

  /**
   * @brief Process parsed SOAP ActionRequest and dispatch to appropriate
   * handler
   * @param action Parsed ActionRequest
   * @param server HttpServer for reply
   * @return true if action handled, false if not supported
   */
  bool processAction(ActionRequest& action, HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNAMediaRenderer::processAction: %s",
                   action.action);
    StrView action_str(action.action);
    if (action_str.isEmpty()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Empty action received");
      server.replyError(400, "Invalid Action");
      return false;
    }

    for (const auto& rule : rules) {
      if (action_str.endsWith(rule.suffix)) {
        return rule.handler(this, action, server);
      }
    }
    DlnaLogger.log(DlnaLogLevel::Error, "Unsupported action: %s",
                   action.action);
    server.replyError(400, "Invalid Action");
    return false;
  }

  /// Setup the action handling rules
  void setupRules() {
    rules.push_back({"Play", [](DLNAMediaRenderer* self, ActionRequest& action,
                                HttpServer& server) {
                       self->setActive(true);
                       NullPrint nop;
                       server.reply("text/xml",
                                    DLNAMediaRenderer::replyPlay(nop),
                                    &DLNAMediaRenderer::replyPlay);
                       return true;
                     }});
    rules.push_back({"Pause", [](DLNAMediaRenderer* self, ActionRequest& action,
                                 HttpServer& server) {
                       self->setActive(false);
                       NullPrint nop;
                       server.reply("text/xml",
                                    DLNAMediaRenderer::replyPause(nop),
                                    &DLNAMediaRenderer::replyPause);
                       return true;
                     }});
    rules.push_back({"Stop", [](DLNAMediaRenderer* self, ActionRequest& action,
                                HttpServer& server) {
                       self->setActive(false);
                       NullPrint nop;
                       server.reply("text/xml",
                                    DLNAMediaRenderer::replyStop(nop),
                                    &DLNAMediaRenderer::replyStop);
                       return true;
                     }});
    rules.push_back({"GetCurrentTransportActions",
                     [](DLNAMediaRenderer* self, ActionRequest& action,
                        HttpServer& server) {
                       StrPrint returnValue;
                       returnValue.printf("<u:return>%s</u:return>",
                                          self->getCurrentTransportActions());
                       StrPrint xml;
                       DLNAMediaRenderer::printReplyXML(
                           xml, "QueryStateVariableResponse", "AVTransport",
                           returnValue.c_str());
                       server.reply("text/xml", xml.c_str(), xml.length());
                       return true;
                     }});
    rules.push_back(
        {"SetAVTransportURI", [](DLNAMediaRenderer* self, ActionRequest& action,
                                 HttpServer& server) {
           const char* uri = action.getArgumentValue("CurrentURI");
           if (uri && *uri) {
             self->setPlaybackURL(uri);
             NullPrint nop;
             server.reply("text/xml",
                          DLNAMediaRenderer::replySetAVTransportURI(nop),
                          &DLNAMediaRenderer::replySetAVTransportURI);
             return true;
           } else {
             DlnaLogger.log(
                 DlnaLogLevel::Warning,
                 "SetAVTransportURI called with invalid SOAP payload");
             server.replyError(400, "Invalid Action");
             return false;
           }
         }});
    rules.push_back(
        {"SetVolume", [](DLNAMediaRenderer* self, ActionRequest& action,
                         HttpServer& server) {
           int desiredVolume = action.getArgumentIntValue("DesiredVolume");
           self->setVolume((uint8_t)desiredVolume);
           NullPrint nop;
           server.reply("text/xml", DLNAMediaRenderer::replySetVolume(nop),
                        &DLNAMediaRenderer::replySetVolume);
           return true;
         }});
    rules.push_back(
        {"SetMute", [](DLNAMediaRenderer* self, ActionRequest& action,
                       HttpServer& server) {
           bool desiredMute = action.getArgumentIntValue("DesiredMute") != 0;
           self->setMuted(desiredMute);
           NullPrint nop;
           server.reply("text/xml", DLNAMediaRenderer::replySetMute(nop),
                        &DLNAMediaRenderer::replySetMute);
           return true;
         }});
    // Add GetMute rule
    rules.push_back({"GetMute", [](DLNAMediaRenderer* self,
                                   ActionRequest& action, HttpServer& server) {
                       StrPrint str;
                       DLNAMediaRenderer::replyGetMute(str, self->isMuted());
                       server.reply("text/xml", str.c_str());
                       return true;
                     }});
    // Add GetVolume rule
    rules.push_back(
        {"GetVolume", [](DLNAMediaRenderer* self, ActionRequest& action,
                         HttpServer& server) {
           StrPrint str;
           DLNAMediaRenderer::replyGetVolume(str, self->getVolume());
           server.reply("text/xml", str.c_str());
           return true;
         }});
  }
};

}  // namespace tiny_dlna
