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
  }

  /**
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
  void end() { dlna_device.end(); }

  /// Call this from Arduino loop()
  bool loop() { return dlna_device.loop(); }

  /// Set the http server instance the MediaRenderer should use
  void setHttpServer(HttpServer& server) {
    p_server = &server;
    // register service endpoints on the provided server
    setupServicesImpl(&server);
  }

  /// Set the UDP service instance the MediaRenderer should use
  void setUdpService(IUDPService& udp) { p_udp_member = &udp; }

  /// Register a media event handler callback
  void setMediaEventHandler(MediaEventHandler cb) { event_cb = cb; }

  /// Set the renderer volume (0..100 percent)
  bool setVolume(uint8_t volumePercent) {
    if (volumePercent > 100) volumePercent = 100;
    DlnaLogger.log(DlnaLogLevel::Info, "Set volume: %d", volumePercent);
    current_volume = volumePercent;
    if (event_cb) event_cb(MediaEvent::SET_VOLUME, *this);
    StrPrint cmd;
    cmd.printf(" <Volume val=\"%d\" channel=\"Master\"/>", volumePercent);
    publishProperty("RCS", cmd.c_str());
    return true;
  }

  /// Get current volume (0..100 percent)
  uint8_t getVolume() { return current_volume; }

  /// Enable or disable mute
  bool setMuted(bool mute) {
    is_muted = mute;
    DlnaLogger.log(DlnaLogLevel::Info, "Set mute: %s", mute ? "true" : "false");
    if (event_cb) event_cb(MediaEvent::SET_MUTE, *this);
    StrPrint cmd;
    cmd.printf("<Mute val=\"%d\" channel=\"Master\"/>", mute ? 1 : 0);
    publishProperty("RCS", cmd.c_str());
    return true;
  }

  /// Query mute state
  bool isMuted() { return is_muted; }

  /// Query whether renderer is active (playing or ready)
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

  // Access current URI
  const char* getCurrentUri() { return current_uri.c_str(); }

  /// Get textual transport state
  const char* getTransportState() { return transport_state.c_str(); }

  /// Provides access to the internal DLNA device instance
  DLNADevice& device() { return dlna_device; }

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

  const char* getCurrentTransportActions() {
    if (is_active) {
      return "Pause,Stop";
    } else {
      return "Play,Stop";
    }
  }

 protected:
  tiny_dlna::Str current_uri;
  tiny_dlna::Str current_mime;
  MediaEventHandler event_cb = nullptr;
  uint8_t current_volume = 50;
  bool is_muted = false;
  bool is_active = false;
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
  // Removed global instance pointer

  /// serviceAbbrev: AVT, RCS, CMS
  void publishProperty(const char* serviceAbbrev, const char* changeTag) {
    SubscriptionMgrDevice* mgr = tiny_dlna::DLNADevice::getSubscriptionMgr();
    DLNAServiceInfo& serviceInfo =
        dlna_device.getServiceByAbbrev(serviceAbbrev);
    if (mgr && serviceInfo) {
      mgr->publishProperty(serviceInfo, changeTag);
    }
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

  /// Transport control handler
  static void transportControlCB(HttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl) {
    // Incremental SOAP parsing using XMLParserPrint to avoid buffering the
    // entire body in RAM. We detect the invoked action name and extract
    // parameters like CurrentURI.
    DLNAMediaRenderer& media_renderer = *((DLNAMediaRenderer*)hl->context[0]);

    XMLParserPrint xp;
    xp.setExpandEncoded(true);
    Str nodeName;
    Vector<Str> path{6};
    Str text;
    Str attrs;

    Str actionName;
    Str currentUri;
    NullPrint nop;


    Client& client = server->client();
    uint8_t buf[XML_PARSER_BUFFER_SIZE];
    while (client.available()) {
      int r = client.read(buf, XML_PARSER_BUFFER_SIZE);
      if (r <= 0) break;
      xp.write(buf, r);
      while (xp.parse(nodeName, path, text, attrs)) {
        // capture action name (Play, Pause, Stop, SetAVTransportURI etc.)
        if (actionName.isEmpty()) {
          if (nodeName.equals("Play") || nodeName.equals("Pause") ||
              nodeName.equals("Stop") ||
              nodeName.equals("GetCurrentTransportActions") ||
              nodeName.equals("SetAVTransportURI")) {
            actionName = nodeName;
          }
        }

        // capture CurrentURI value
        if (nodeName.equals("CurrentURI") && !text.isEmpty()) {
          currentUri = text;
        }
      }
    }

    // Use printReplyXML via a non-capturing callback so the HttpServer can
    // call a function pointer. The instance is accessed through
    // p_media_renderer which is set in setHttpServer().
    if (actionName == "Play") {
      media_renderer.setActive(true);
      server->reply("text/xml", DLNAMediaRenderer::replyPlay(nop),&DLNAMediaRenderer::replyPlay);
      return;
    }
    if (actionName == "Pause") {
      media_renderer.setActive(false);
      server->reply("text/xml", DLNAMediaRenderer::replyPause(nop), &DLNAMediaRenderer::replyPause);
      return;
    }
    if (actionName == "Stop") {
      media_renderer.setActive(false);
      server->reply("text/xml", DLNAMediaRenderer::replyStop(nop), &DLNAMediaRenderer::replyStop);
      return;
    }
    if (actionName == "GetCurrentTransportActions") {
      StrPrint returnValue;
      returnValue.printf("<u:return>%s</u:return>",
                         media_renderer.getCurrentTransportActions());
      StrPrint xml;
      DLNAMediaRenderer::printReplyXML(xml, "QueryStateVariableResponse",
                                       "AVTransport", returnValue.c_str());
      server->reply("text/xml", xml.c_str(), xml.length());
      return;
    }
    if (actionName == "SetAVTransportURI") {
      if (!currentUri.isEmpty()) {
        media_renderer.setPlaybackURL(currentUri.c_str());
        server->reply("text/xml", DLNAMediaRenderer::replySetAVTransportURI(nop), &DLNAMediaRenderer::replySetAVTransportURI);
        return;
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "SetAVTransportURI called with invalid SOAP payload");
      }
    }
    server->replyError(400, "Invalid Action");
  }

  /// Rendering control handler
  static void renderingControlCB(HttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl) {
    // Incremental SOAP parsing: detect SetVolume/SetMute and extract values
    DLNAMediaRenderer& media_renderer = *((DLNAMediaRenderer*)hl->context[0]);

    XMLParserPrint xp;
    xp.setExpandEncoded(true);
    Str nodeName;
    Vector<Str> path{6};
    Str text;
    Str attrs;
    NullPrint nop;

    Str actionName;
    int desiredVolume = -1;
    bool desiredMute = false;
    bool haveVolume = false;
    bool haveMute = false;

    Client& client = server->client();
    uint8_t buf[XML_PARSER_BUFFER_SIZE];
    while (client.available()) {
      int r = client.read(buf, XML_PARSER_BUFFER_SIZE);
      if (r <= 0) break;
      xp.write(buf, r);
      while (xp.parse(nodeName, path, text, attrs)) {
        if (actionName.isEmpty()) {
          if (nodeName.equals("SetVolume") || nodeName.equals("SetMute")) {
            actionName = nodeName;
          }
        }
        if (nodeName.equals("DesiredVolume") && !text.isEmpty()) {
          desiredVolume = text.toInt();
          haveVolume = true;
        }
        if (nodeName.equals("DesiredMute") && !text.isEmpty()) {
          desiredMute = (text == "1");
          haveMute = true;
        }
      }
    }

    // Reply using printReplyXML via a non-capturing callback so the
    // HttpServer can use a plain function pointer. The instance is
    // accessed through p_media_renderer.
    if (actionName == "SetVolume" && haveVolume) {
      media_renderer.setVolume((uint8_t)desiredVolume);
      server->reply("text/xml", DLNAMediaRenderer::replySetVolume(nop),
                    &DLNAMediaRenderer::replySetVolume);
      return;
    }
    if (actionName == "SetMute" && haveMute) {
      media_renderer.setMuted(desiredMute);
      server->reply("text/xml", DLNAMediaRenderer::replySetMute(nop),
                    &DLNAMediaRenderer::replySetMute);
      return;
    }
    server->replyError(400, "Invalid Action");
  };

  /// Setup service descriptors and control/event endpoints
  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::setupServices");

    auto transportCB = [](HttpServer* server, const char* requestPath,
                          HttpRequestHandlerLine* hl) {
      server->reply("text/xml", [](Print& out) {
        DLNAMediaRendererTransportDescr d;
        d.printDescr(out);
      });
    };

    auto connmgrCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", [](Print& out) {
        DLNAMediaRendererConnectionMgrDescr d;
        d.printDescr(out);
      });
    };

    auto controlCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", [](Print& out) {
        DLNAMediaRendererControlDescr d;
        d.printDescr(out);
      });
    };

    // define services
    DLNAServiceInfo rc, cm, avt;
    avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
              transportCB, "/AVT/control", transportControlCB, "/AVT/event",
              [](HttpServer* server, const char* requestPath,
                 HttpRequestHandlerLine* hl) { server->replyOK(); });
    avt.subscription_namespace_abbrev = "AVT";

    cm.setup(
        "urn:schemas-upnp-org:service:ConnectionManager:1",
        "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
        connmgrCB, "/CM/control",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); },
        "/CM/event",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); });
    cm.subscription_namespace_abbrev = "CMS";

    rc.setup("urn:schemas-upnp-org:service:RenderingControl:1",
             "urn:upnp-org:serviceId:RenderingControl", "/RC/service.xml",
             controlCB, "/RC/control", renderingControlCB, "/RC/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });
    rc.subscription_namespace_abbrev = "RCS";

    addService(rc);
    addService(cm);
    addService(avt);
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
};

}  // namespace tiny_dlna
