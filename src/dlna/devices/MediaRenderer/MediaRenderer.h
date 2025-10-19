#pragma once
#include <cctype>
#include <cstring>

#include "basic/Str.h"
#include "dlna/DLNADeviceMgr.h"
#include "dlna/xml/XMLAttributeParser.h"
#include "mr_conmgr.h"
#include "mr_control.h"
#include "mr_transport.h"

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
class MediaRenderer : public DLNADevice {
 public:
  // event handler: (event, reference to MediaRenderer)
  typedef void (*MediaEventHandler)(MediaEvent event, MediaRenderer& renderer);

  /**
   * @brief Default constructor
   *
   * Initializes device metadata (friendly name, manufacturer, model) and
   * sets default base URL and identifiers. It does not configure any audio
   * pipeline components; use setOutput() and setDecoder() for that.
   */
  MediaRenderer() {
    // Constructor
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::MediaRenderer");
    setSerialNumber(usn);
    setDeviceType(st);
    setFriendlyName("MediaRenderer");
    setManufacturer("TinyDLNA");
    setModelName("TinyDLNA");
    setModelNumber("1.0");
    setBaseURL("http://localhost:44757");
  }

  /// Register a media event handler callback
  void setMediaEventHandler(MediaEventHandler cb) { event_cb = cb; }

  /// Set the renderer volume (0..100 percent)
  bool setVolume(uint8_t volumePercent) {
    if (volumePercent > 100) volumePercent = 100;
    DlnaLogger.log(DlnaLogLevel::Info, "Set volume: %d", volumePercent);
    current_volume = volumePercent;
    if (event_cb) event_cb(MediaEvent::SET_VOLUME, *this);
    return true;
  }

  /// Get current volume (0..100 percent)
  uint8_t getVolume() { return current_volume; }

  /// Enable or disable mute
  bool setMute(bool mute) {
    is_muted = mute;
    DlnaLogger.log(DlnaLogLevel::Info, "Set mute: %s", mute ? "true" : "false");
    if (event_cb) event_cb(MediaEvent::SET_MUTE, *this);
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
  }

  /// Get estimated playback position (ms)
  unsigned long getPosition() {
    // Estimate position; return 0 if not active or start_time not set
    if (!is_active || start_time == 0) return 0;
    return millis() - start_time;
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

 protected:
  tiny_dlna::Str current_uri;
  tiny_dlna::Str current_mime;
  MediaEventHandler event_cb = nullptr;
  uint8_t current_volume = 50;
  bool is_muted = false;
  bool is_active = false;
  unsigned long start_time = 0;
  // Current transport state string (e.g. "STOPPED", "PLAYING", "PAUSED_PLAYBACK")
  tiny_dlna::Str transport_state = "NO_MEDIA_PRESENT";
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "uuid:09349455-2941-4cf7-9847-1dd5ab210e97";

  /// Setup the HTTP and UDP services
  void setupServices(HttpServer& server, IUDPService& udp) {
    setupServicesImpl(&server);
  }

  /// Start playback of a network resource (returns true on success)
  bool play(const char* urlStr) {
    if (urlStr == nullptr) return false;
    DlnaLogger.log(DlnaLogLevel::Info, "play URL: %s", urlStr);
    // store URI
    current_uri = Str(urlStr);
    // notify handler about the new URI and play
    if (event_cb) {
      event_cb(MediaEvent::SET_URI, *this);
      event_cb(MediaEvent::PLAY, *this);
    }
    is_active = true;
    start_time = millis();
    return true;
  }

  /// Set MIME explicitly (used when DIDL-Lite metadata provides protocolInfo)
  void setMime(const char* mime) {
    if (mime) {
      current_mime = mime;
      DlnaLogger.log(DlnaLogLevel::Info, "Set mime: %s", current_mime.c_str());
    }
  }

  /**
   * @brief Notify the renderer that playback completed.
   *
   * This helper updates the internal transport state to "STOPPED",
   * marks the renderer inactive, resets the start time, logs the event and
   * notifies the application event handler with MediaEvent::STOP so the
   * application can perform cleanup (release resources, update UI, etc.).
   */
  void playbackCompleted() {
    DlnaLogger.log(DlnaLogLevel::Info, "Playback completed");
    transport_state = "STOPPED";
    is_active = false;
    start_time = 0;
    // notify application handler about the stop
    if (event_cb) event_cb(MediaEvent::STOP, *this);
    // publish UPnP event to subscribers (if subscription manager available)
    SubscriptionMgr* mgr = tiny_dlna::DLNADeviceMgr::getSubscriptionMgr();
    if (mgr) {
      mgr->publishProperty("/AVT/event", "TransportState", "STOPPED");
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

  static const char* reply() {
    static const char* result =
        "<s:Envelope "
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/"
        "\"><s:Body><u:%1 "
        "xmlns:u=\"urn:schemas-upnp-org:service:%2:"
        "1\"></u:%1></s:Body></s:Envelope>";
    return result;
  }

  // Transport control handler
  static void transportControlCB(HttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl) {
    // Parse SOAP request and extract action
    Str soap = server->contentStr();
    DlnaLogger.log(DlnaLogLevel::Info, "Transport Control: %s", soap.c_str());
    MediaRenderer& media_renderer = *((MediaRenderer*)hl->context[0]);

    Str reply_str{reply()};
    reply_str.replaceAll("%2", "AVTransport");
    if (soap.indexOf("Play") >= 0) {
      media_renderer.setActive(true);
      reply_str.replaceAll("%1", "PlayResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("Pause") >= 0) {
      media_renderer.setActive(false);
      reply_str.replaceAll("%1", "PauseResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("Stop") >= 0) {
      media_renderer.setActive(false);
      reply_str.replaceAll("%1", "StopResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("SetAVTransportURI") >= 0) {
      // Extract URL from SOAP request safely
      int urlTagStart = soap.indexOf("<CurrentURI>");
      int urlTagEnd = soap.indexOf("</CurrentURI>");
      if (urlTagStart >= 0 && urlTagEnd > urlTagStart) {
        int urlStart = urlTagStart + (int)strlen("<CurrentURI>");
        Str url = soap.substring(urlStart, urlTagEnd);
        media_renderer.play(url.c_str());
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "SetAVTransportURI called with invalid SOAP payload");
      }
      reply_str.replaceAll("%1", "SetAVTransportURIResponse");
      server->reply("text/xml", reply_str.c_str());
    } else {
      // Handle other transport controls
      reply_str.replaceAll("%1", "ActionResponse");
      server->reply("text/xml", reply_str.c_str());
    }
  }

  // Rendering control handler
  static void renderingControlCB(HttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl) {
    // Parse SOAP request and extract action
    MediaRenderer& media_renderer = *((MediaRenderer*)hl->context[0]);
    Str soap = server->contentStr();
    DlnaLogger.log(DlnaLogLevel::Info, "Rendering Control: %s", soap.c_str());
    Str reply_str{reply()};
    reply_str.replaceAll("%2", "RenderingControl");

    if (soap.indexOf("SetVolume") >= 0) {
      // Extract volume from SOAP request safely
      int volTagStart = soap.indexOf("<DesiredVolume>");
      int volTagEnd = soap.indexOf("</DesiredVolume>");
      if (volTagStart >= 0 && volTagEnd > volTagStart) {
        int volStart = volTagStart + (int)strlen("<DesiredVolume>");
        int volume = soap.substring(volStart, volTagEnd).toInt();
        media_renderer.setVolume(volume);
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "SetVolume called with invalid SOAP payload");
      }
      reply_str.replaceAll("%1", "SetVolumeResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("SetMute") >= 0) {
      // Extract mute state from SOAP request safely
      int muteTagStart = soap.indexOf("<DesiredMute>");
      int muteTagEnd = soap.indexOf("</DesiredMute>");
      if (muteTagStart >= 0 && muteTagEnd > muteTagStart) {
        int muteStart = muteTagStart + (int)strlen("<DesiredMute>");
        bool mute = (soap.substring(muteStart, muteTagEnd) == "1");
        media_renderer.setMute(mute);
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "SetMute called with invalid SOAP payload");
      }
      reply_str.replaceAll("%1", "SetMuteResponse");
      server->reply("text/xml", reply_str.c_str());
    } else {
      // Handle other rendering controls
      reply_str.replaceAll("%1", "RenderingControl");
      server->reply("text/xml", reply_str.c_str());
    }
  };

  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::setupServices");

    auto transportCB = [](HttpServer* server, const char* requestPath,
                          HttpRequestHandlerLine* hl) {
            server->reply("text/xml", [](Print& out){ mr_connmgr_xml_printer(out); });
    };

    auto connmgrCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", [](Print& out){ mr_connmgr_xml_printer(out); });
    };

    auto controlCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", [](Print& out){ mr_control_xml_printer(out); });
    };

    // define services
    DLNAServiceInfo rc, cm, avt;
    avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
              transportCB, "/AVT/control", transportControlCB, "/AVT/event",
              [](HttpServer* server, const char* requestPath,
                 HttpRequestHandlerLine* hl) { server->replyOK(); });

    cm.setup(
        "urn:schemas-upnp-org:service:ConnectionManager:1",
        "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
        connmgrCB, "/CM/control",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); },
        "/CM/event",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); });

    rc.setup("urn:schemas-upnp-org:service:RenderingControl:1",
             "urn:upnp-org:serviceId:RenderingControl", "/RC/service.xml",
             controlCB, "/RC/control", renderingControlCB, "/RC/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });

    addService(rc);
    addService(cm);
    addService(avt);
  }
};

}  // namespace tiny_dlna
