#pragma once
#include <cctype>
#include <cstring>
#include <functional>

#include "DLNAMediaRendererDescr.h"
#include "basic/Printf.h"
#include "basic/Str.h"
#include "dlna/devices/DLNADevice.h"
#include "dlna/udp/IUDPService.h"
#include "dlna/xml/XMLAttributeParser.h"
#include "dlna/xml/XMLParserPrint.h"
#include "http/Server/IHttpServer.h"

namespace tiny_dlna {
/**
 * @enum MediaEvent
 * @brief Events emitted by the MediaRenderer to notify the application
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
 * The template parameter declares the HTTP client type to use for outgoing
 * control and subscription traffic. Supply the client that matches the
 * networking stack in your sketch (for example WiFiClient).
 *
 * @tparam ClientType Arduino `Client` implementation used for outbound HTTP
 *         control and event traffic (e.g. `WiFiClient`, `EthernetClient`).
 *
 * Author: Phil Schatzmann
 */
template <typename ClientType>
class DLNAMediaRenderer : public DLNADeviceInfo {
 public:
  using HttpClient = ClientType;
  using DeviceType = DLNADevice<ClientType>;
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
    dlna_device.setReference(this);
  }

  /**
   *  @brief Recommended constructor
   * Construct a MediaRenderer bound to an HTTP server and IUDPService.
   * The provided references are stored and used when calling begin().
   */
  DLNAMediaRenderer(IHttpServer& server, IUDPService& udp)
      : DLNAMediaRenderer() {
    setHttpServer(server);
    setUdpService(udp);
  }

  /// Start the underlying DLNA device using the stored server/udp
  bool begin() { return dlna_device.begin(*this, *p_udp_member, *p_server); }

  /// Stops processing and releases resources
  void end() {
    if (is_active) stop();
    dlna_device.end();
  }

  /// Call this from Arduino loop()
  bool loop(int loopAction = RUN_ALL) { return dlna_device.loop(loopAction); }

  /// Set the http server instance the MediaRenderer should use
  void setHttpServer(IHttpServer& server) {
    p_server = &server;
    // prepare handler context and register each service via helpers
    server.setReference(this);
    setupServicesImpl(&server);
  }

  /// Set the UDP service instance the MediaRenderer should use
  void setUdpService(IUDPService& udp) { p_udp_member = &udp; }

  /// Set possible playback storage media (comma-separated list)
  void setPossiblePlaybackStorageMedia(const char* v) {
    possiblePlaybackStorageMedia = v ? v : "";
  }

  /// Get possible playback storage media
  const char* getPossiblePlaybackStorageMedia() const {
    return possiblePlaybackStorageMedia;
  }

  /// Set possible record storage media (comma-separated list)
  void setPossibleRecordStorageMedia(const char* v) {
    possibleRecordStorageMedia = v ? v : "";
  }

  /// Get possible record storage media
  const char* getPossibleRecordStorageMedia() const {
    return possibleRecordStorageMedia;
  }

  /// Set possible record quality modes (comma-separated list)
  void setPossibleRecordQualityModes(const char* v) {
    possibleRecordQualityModes = v ? v : "";
  }

  /// Get possible record quality modes
  const char* getPossibleRecordQualityModes() const {
    return possibleRecordQualityModes;
  }

  /// Set current play mode (e.g. NORMAL, REPEAT_ALL, INTRO)
  void setPlayMode(const char* v) { currentPlayMode = v ? v : "NORMAL"; }

  /// Get current play mode
  const char* getPlayMode() const { return currentPlayMode; }

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
    // reflect state in transport_state so deferred writer can read it
    transport_state = active ? "PLAYING" : "PAUSED_PLAYBACK";
    struct StateWriter {
      static size_t write(Print& out, void* ref) {
        auto self = (DLNAMediaRenderer*)ref;
        size_t written = 0;

        written += out.print("<TransportState val=\"");
        written += out.print(self->getTransportState());
        written += out.print("\"/>");
        written += out.print("<CurrentTransportActions val=\"");
        written += out.print(self->getCurrentTransportActions());
        written += out.print("\"/>");

        return written;
      }
    };
    addChange("AVT", StateWriter::write);
  }

  /// Provides the mime from the DIDL or nullptr
  const char* getMime() {
    // Prefer explicit MIME from DIDL if available
    if (!current_mime.isEmpty()) return current_mime.c_str();
    return nullptr;
  }

  /// Register application event handler
  void setMediaEventHandler(MediaEventHandler cb) { event_cb = cb; }

  /// Get current volume (0-100)
  uint8_t getVolume() { return current_volume; }

  /// Set volume and publish event (0-100)
  void setVolume(uint8_t vol) {
    current_volume = vol;
    (void)current_volume;  // value used by writer via ref
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t written = 0;
      written += out.print("<Volume val=\"");
      written += out.print(self->current_volume);
      written += out.print("\" channel=\"Master\"/>");
      return written;
    };
    addChange("RCS", writer);
  }

  /// Query mute state
  bool isMuted() { return getVolume() == 0; }

  /// Set mute state and publish event
  void setMuted(bool mute) {
    if (mute) {
      // store current volume and set to 0
      muted_volume = current_volume;
      current_volume = 0;
    } else {
      // restore previous volume
      current_volume = muted_volume;
    }
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t written = 0;
      written += out.print("<Mute val=\"");
      written += out.print(self->isMuted() ? 1 : 0);
      written += out.print("\" channel=\"Master\"/>");
      return written;
    };
    addChange("RCS", writer);
  }

  /// Access current URI
  const char* getCurrentUri() { return current_uri.c_str(); }

  /// Access the metadata as string
  const char* getCurrentUriMetadata() { return current_uri_metadata.c_str(); }

  /// Get textual transport state
  const char* getTransportState() { return transport_state.c_str(); }

  /// Provides access to the internal DLNA device instance
  IDevice& device() { return dlna_device; }

  /// Enable/disable subscription notifications
  void setSubscriptionsActive(bool flag) {
    dlna_device.setSubscriptionsActive(flag);
  }

  /// Query whether subscription notifications are active
  bool isSubscriptionsActive() { return dlna_device.isSubscriptionsActive(); }

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
    // notify handler about the new URI; do not dispatch PLAY here — action
    // handlers will dispatch media events explicitly.
    is_active = true;

    // ensure transport_state reflects playing for subscribers
    transport_state = "PLAYING";
    (void)current_uri;  // writer will fetch it from ref
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t written = 0;
      written += out.print("<CurrentTrack val=\"1\"/>\n");
      written += out.print("<CurrentTrackURI val=\"");
      written += out.print(self->current_uri.c_str());
      written += out.println("\"/>");
      written += out.print("<CurrentTrackMetadata val=\"");
      written += out.print(self->current_uri_metadata.c_str());
      written += out.println("\"/>");
      written += out.print("<TransportState val=\"");
      written += out.print(self->transport_state.c_str());
      written += out.print("\"/>");
      written += out.print("<CurrentTransportActions val=\"");
      written += out.print(self->transport_actions.c_str());
      written += out.print("\"/>");
      return written;
    };
    addChange("AVT", writer);

    return true;
  }

  /// Defines the actual url to play
  bool setPlaybackURL(const char* urlStr) {
    if (urlStr == nullptr) return false;
    DlnaLogger.log(DlnaLogLevel::Info, "setPlaybackURL URL: %s", urlStr);
    // store URI
    current_uri = Str(urlStr);
    // Do not notify application here; event notifications are emitted
    // from the SOAP action handlers (processAction*).
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t written = 0;
      written += out.print("<AVTransportURI val=\"");
      written += out.print(self->current_uri.c_str());
      written += out.println("\"/>");

      written += out.print("<AVTransportURIMetaData val=\"");
      written += out.print(self->current_uri_metadata.c_str());
      written += out.println("\"/>");

      written += out.print("<NumberOfTracks val=\"1\"/>\n");
      return written;
    };
    addChange("AVT", writer);

    return true;
  }

  /// Stop playback
  bool stop() {
    DlnaLogger.log(DlnaLogLevel::Info, "Stop playback");
    is_active = false;
    start_time = 0;
    time_sum = 0;
    // reflect stopped state
    transport_state = "STOPPED";
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t written = 0;
      written += out.print("<TransportState val=\"");
      written += out.print(self->getTransportState());
      written += out.print("\"/>");
      written += out.print("<CurrentTransportActions val=\"");
      written += out.print(self->getCurrentTransportActions());
      written += out.print("\"/>");
      return written;
    };
    addChange("AVT", writer);
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
    // Do not notify application here; event notifications are emitted
    // from the SOAP action handlers (processAction*).
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t written = 0;
      written += out.print("<TransportState val=\"");
      written += out.print(self->transport_state.c_str());
      written += out.print("\"/>");
      written += out.print("<CurrentPlayMode val=\"NORMAL\"/>");
      written += out.print("<CurrentTrackURI val=\"\"/>");
      written += out.print("<RelativeTimePosition val=\"00:00:00\"/>");
      written += out.print("<CurrentTransportActions val=\"Play\"/>");
      return written;
    };
    addChange("AVT", writer);
  }

  /// Get estimated playback position (seconds)
  size_t getRelativeTimePositionSec() {
    unsigned long posMs = (millis() - start_time) + time_sum;
    return static_cast<size_t>(posMs / 1000);
  }

  /// Publish the RelativeTimePosition property
  void publishGetRelativeTimePositionSec() {
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t written = 0;
      // format hh:mm:ss by printing parts
      int h = static_cast<int>(self->getRelativeTimePositionSec() / 3600);
      int m =
          static_cast<int>((self->getRelativeTimePositionSec() % 3600) / 60);
      int s = static_cast<int>(self->getRelativeTimePositionSec() % 60);
      written += out.print("<RelativeTimePosition val=\"");
      // print two-digit hour
      if (h < 10) written += out.print('0');
      written += out.print(h);
      written += out.print(':');
      if (m < 10) written += out.print('0');
      written += out.print(m);
      written += out.print(':');
      if (s < 10) written += out.print('0');
      written += out.print(s);
      written += out.print("\"/>");
      return written;
    };
    addChange("AVT", writer);
  }

  /// Get a csv of the valid actions
  const char* getCurrentTransportActions() {
    if (current_uri.isEmpty()) {
      return "SetAVTransportURI";
    }
    if (is_active) {
      return "Pause,Stop";
    } else {
      return "Play";
    }
  }
  /// Defines a custom action rule for the media renderer.
  void setCustomActionRule(const char* suffix,
                           bool (*handler)(DLNAMediaRenderer*, ActionRequest&,
                                           IHttpServer&)) {
    for (size_t i = 0; i < rules.size(); ++i) {
      if (StrView(rules[i].suffix).equals(suffix)) {
        rules[i].handler = handler;
        return;
      }
    }
    rules.push_back({suffix, handler});
  }

  /// Set the active ConnectionID for the connection manager
  void setConnectionID(const char* id) { connectionID = id; }

  /// Return the currently configured ConnectionID
  const char* getConnectionID() { return connectionID; }

  /**
   * @brief Define the source protocol info: use csv
   * Default is
   * http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN,http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_SM,http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED,http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_LRG,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_HD_50_AC3_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_HD_60_AC3_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_PS_NTSC,http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_PS_PAL,http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_TS_HD_NA_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_TS_SD_NA_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_TS_SD_EU_ISO,http-get:*:video/mpeg:DLNA.ORG_PN=MPEG1,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AAC_MULT5,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AC3,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF15_AAC_520,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF30_AAC_940,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L31_HD_AAC,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L32_HD_AAC,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_HP_HD_AAC,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_1080i_AAC,http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_720p_AAC,http-get:*:video/mp4:DLNA.ORG_PN=MPEG4_P2_MP4_ASP_AAC,http-get:*:video/mp4:DLNA.ORG_PN=MPEG4_P2_MP4_SP_VGA_AAC,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU_T,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA,http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA_T,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVSPLL_BASE,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVSPML_BASE,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVSPML_MP3,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVMED_BASE,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVMED_FULL,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVMED_PRO,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVHIGH_FULL,http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVHIGH_PRO,http-get:*:video/3gpp:DLNA.ORG_PN=MPEG4_P2_3GPP_SP_L0B_AAC,http-get:*:video/3gpp:DLNA.ORG_PN=MPEG4_P2_3GPP_SP_L0B_AMR,http-get:*:audio/mpeg:DLNA.ORG_PN=MP3,http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMABASE,http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMAFULL,http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMAPRO,http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMALSL,http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMALSL_MULT5,http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO_320,http-get:*:audio/3gpp:DLNA.ORG_PN=AAC_ISO_320,http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO,http-get:*:audio/mp4:DLNA.ORG_PN=AAC_MULT5_ISO,http-get:*:audio/L16;rate=44100;channels=2:DLNA.ORG_PN=LPCM,http-get:*:image/jpeg:*,http-get:*:video/avi:*,http-get:*:video/divx:*,http-get:*:video/x-matroska:*,http-get:*:video/mpeg:*,http-get:*:video/mp4:*,http-get:*:video/x-ms-wmv:*,http-get:*:video/x-msvideo:*,http-get:*:video/x-flv:*,http-get:*:video/x-tivo-mpeg:*,http-get:*:video/quicktime:*,http-get:*:audio/mp4:*,http-get:*:audio/x-wav:*,http-get:*:audio/x-flac:*,http-get:*:audio/x-dsd:*,http-get:*:application/ogg:*http-get:*:application/vnd.rn-realmedia:*http-get:*:application/vnd.rn-realmedia-vbr:*http-get:*:video/webm:*
   */
  void setProtocols(const char* source, const char* sink = "") {
    sourceProto = source;
    sinkProto = sink;
    // publish protocol info change to ConnectionManager subscribers
    // Build writer that will use live state from the device at publish time
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      size_t result = 0;
      result += out.print("<SourceProtocolInfo val=\"");
      result += out.print(StrView(self->getSourceProtocols()).c_str());
      result += out.println("\"/>");
      result += out.print("<SinkProtocolInfo val=\"");
      result += out.print(StrView(self->getSinkProtocols()).c_str());
      result += out.println("\"/>");
      return result;
    };
    addChange("CMS", writer);
  }

  /// Get the current source `ProtocolInfo` string
  const char* getSourceProtocols() { return sourceProto; }

  /// Get the current sink `ProtocolInfo` string
  const char* getSinkProtocols() { return sinkProto; }

  /// Set the transport SCPD descriptor (non-owning reference).
  void setTransportDescr(DLNADescr& d) { p_transportDescr = &d; }

  /// Get the transport SCPD descriptor reference.
  DLNADescr& getTransportDescr() { return *p_transportDescr; }

  /// Set the control SCPD descriptor (non-owning reference).
  void setControlDescr(DLNADescr& d) { p_controlDescr = &d; }

  /// Get the control SCPD descriptor reference.
  DLNADescr& getControlDescr() { return *p_controlDescr; }

  /// Set the ConnectionManager SCPD descriptor (non-owning reference).
  void setConnectionMgrDescr(DLNADescr& d) { p_connmgrDescr = &d; }

  /// Get the ConnectionManager SCPD descriptor reference.
  DLNADescr& getConnectionMgrDescr() { return *p_connmgrDescr; }

 protected:
  /// Individual action rule for handling specific actions
  struct ActionRule {
    const char* suffix;
    bool (*handler)(DLNAMediaRenderer*, ActionRequest&, IHttpServer&);
  };

  tiny_dlna::Str current_uri;
  tiny_dlna::Str current_mime;
  tiny_dlna::Str current_uri_metadata;
  MediaEventHandler event_cb = nullptr;
  uint8_t current_volume = 50;
  uint8_t muted_volume = 0;
  unsigned long start_time = 0;
  unsigned long time_sum = 0;
  // Current state (e.g. "STOPPED", "PLAYING","PAUSED_PLAYBACK")
  tiny_dlna::Str transport_state = "STOPPED";
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "uuid:09349455-2941-4cf7-9847-1dd5ab210e97";
  DeviceType dlna_device;
  IHttpServer* p_server = nullptr;
  IUDPService* p_udp_member = nullptr;
  const char* connectionID = "0";
  const char* sourceProto = "";
  const char* sinkProto = DLNA_PROTOCOL_AUDIO;
  const char* possiblePlaybackStorageMedia = "NETWORK";
  const char* possibleRecordStorageMedia = "NONE";
  const char* possibleRecordQualityModes = "NOT_IMPLEMENTED";
  const char* currentPlayMode = "NORMAL";
  Vector<ActionRule> rules;

  // Descriptor instances so callers can customise the SCPD output per device
  DLNAMediaRendererTransportDescr default_transport_desc;
  DLNADescr* p_transportDescr = &default_transport_desc;
  DLNAMediaRendererControlDescr default_control_desc;
  DLNADescr* p_controlDescr = &default_control_desc;
  DLNAMediaRendererConnectionMgrDescr default_connmgr_desc;
  DLNADescr* p_connmgrDescr = &default_connmgr_desc;

  // Note: descriptor printing is performed inline in the descriptor
  // callbacks below using non-capturing lambdas passed to
  // IHttpServer::reply. That avoids heap buffering while keeping the
  // callsites concise. The old static helper functions were removed.

  /// serviceAbbrev: AVT, RCS, CMS
  void addChange(const char* serviceAbbrev,
                 std::function<size_t(Print&, void*)> changeWriter) {
    dlna_device.addChange(serviceAbbrev, changeWriter, this);
  }

  /// Publish the current AVTransport state (TransportState, CurrentTrackURI,
  /// RelativeTimePosition, CurrentTransportActions)
  void publishAVT() {
    // Transport state will be built by the writer from the instance (ref)
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      Printf outf{out};
      size_t written = 0;
      written += outf.printf("<TransportState val=\"%s\"/>",
                             self->transport_state.c_str());
      if (!self->current_uri.isEmpty()) {
        written += outf.printf("<CurrentTrackURI val=\"%s\"/>",
                               self->current_uri.c_str());
      }
      written += outf.printf(
          "<RelativeTimePosition val=\"%02d:%02d:%02d\"/>",
          static_cast<int>(self->getRelativeTimePositionSec() / 3600),
          static_cast<int>((self->getRelativeTimePositionSec() % 3600) / 60),
          static_cast<int>(self->getRelativeTimePositionSec() % 60));
      written += outf.printf("<CurrentTransportActions val=\"%s\"/>",
                             self->getCurrentTransportActions());
      return written;
    };
    addChange("AVT", writer);
  }

  /// Publish the current RenderingControl state (Volume, Mute)
  void publishRCS() {
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaRenderer*)ref;
      Printf outf{out};
      size_t written = 0;
      written += outf.printf("<Volume val=\"%d\" channel=\"Master\"/>", self->current_volume);
      written += out.println();
      written += outf.printf("<Mute val=\"%d\" channel=\"Master\"/>", self->isMuted() ? 1 : 0);
      written += out.println();
      return written;
    };
    addChange("RCS", writer);
  }

  /// Publish a minimal ConnectionManager state (CurrentConnectionIDs)
  void publishCMS() {
    auto writer = [](Print& out, void* ref) -> size_t {
      DLNAMediaRenderer* self = (DLNAMediaRenderer*)ref;
      Printf printer{out};
      return printer.printf("<CurrentConnectionIDs>%s</CurrentConnectionIDs>");
    };
    addChange("CMS", writer);
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
    char mimeBuf[256] = {0};
    if (XMLAttributeParser::extractAttributeToken(
            didl, "<res", "protocolInfo=", 3, mimeBuf, sizeof(mimeBuf))) {
      setMime(mimeBuf);
    }
  }

  // Static descriptor callbacks: print the SCPD XML for each service
  static void transportDescrCB(IHttpServer* server, const char* requestPath,
                               HttpRequestHandlerLine* hl) {
    DLNAMediaRenderer* self = getRenderer(server);
    if (self) {
      // Use the Print+ref reply overload to stream directly into the
      // client's Print without heap buffering. Pass `self` as ref so the
      // callback can route to the correct instance. Use an inline
      // non-capturing lambda so we don't need a separate static helper.
      server->reply(
          "text/xml",
          [](Print& out, void* ref) -> size_t {
            size_t result = 0;
            auto self = static_cast<DLNAMediaRenderer*>(ref);
            if (self) result += self->getTransportDescr().printDescr(out);
            return result;
          },
          200, nullptr, self);
    } else {
      DlnaLogger.log(DlnaLogLevel::Error,
                     "transportDescrCB: renderer instance not found for %s",
                     requestPath);
      server->replyNotFound();
    }
  }

  static void connmgrDescrCB(IHttpServer* server, const char* requestPath,
                             HttpRequestHandlerLine* hl) {
    DLNAMediaRenderer* self = getRenderer(server);
    if (self) {
      server->reply(
          "text/xml",
          [](Print& out, void* ref) -> size_t {
            size_t result = 0;
            auto self = static_cast<DLNAMediaRenderer*>(ref);
            if (self) result += self->getConnectionMgrDescr().printDescr(out);
            return result;
          },
          200, nullptr, self);
    } else {
      DlnaLogger.log(DlnaLogLevel::Error,
                     "connmgrDescrCB: renderer instance not found for %s",
                     requestPath);
      server->replyNotFound();
    }
  }

  static void controlDescrCB(IHttpServer* server, const char* requestPath,
                             HttpRequestHandlerLine* hl) {
    DLNAMediaRenderer* self = getRenderer(server);
    if (self) {
      server->reply(
          "text/xml",
          [](Print& out, void* ref) -> size_t {
            size_t result = 0;
            auto self = static_cast<DLNAMediaRenderer*>(ref);
            if (self) result += self->getControlDescr().printDescr(out);
            return result;
          },
          200, nullptr, self);
    } else {
      DlnaLogger.log(DlnaLogLevel::Error,
                     "controlDescrCB: renderer instance not found for %s",
                     requestPath);
      server->replyNotFound();
    }
  }

  // Generic control callback: parse SOAP action and dispatch to instance
  static void controlCB(IHttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DeviceType::parseActionRequest(server, requestPath, hl, action);

    DLNAMediaRenderer* self = getRenderer(server);

    // Log incoming SOAPAction header and parsed action for debugging
    const char* soapHdr = server->requestHeader().get("SOAPACTION");
    DlnaLogger.log(DlnaLogLevel::Debug, "controlCB: SOAPAction=%s, action=%s",
                   StrView(soapHdr).c_str(),
                   StrView(action.getAction()).c_str());

    if (self) {
      if (self->processAction(action, *server)) return;
    }

    server->replyNotFound();
  }

  /// Determine the Renderer with the help of the server
  static DLNAMediaRenderer* getRenderer(IHttpServer* server) {
    auto* p_device = static_cast<IDevice*>(server->getReference());
    if (p_device == nullptr) return nullptr;
    return static_cast<DLNAMediaRenderer*>(p_device->getReference());
  }

  /// After the subscription we publish all relevant properties
  static void eventSubscriptionHandler(IHttpServer* server,
                                       const char* requestPath,
                                       HttpRequestHandlerLine* hl) {
    bool is_subscribe = false;
    DeviceType::handleSubscription(server, requestPath, hl, is_subscribe);
    if (is_subscribe) {
      DLNAMediaRenderer* self = getRenderer(server);
      assert(self != nullptr);
      StrView request_path_str(requestPath);
      if (request_path_str.contains("/AVT/"))
        self->publishAVT();
      else if (request_path_str.contains("/RC/"))
        self->publishRCS();
      else if (request_path_str.contains("/CM/"))
        self->publishCMS();
    }
  }

  // Setup and register individual services
  void setupTransportService(IHttpServer* server) {
    DLNAServiceInfo avt;
    // Use static descriptor callback and control handler
    avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
              &DLNAMediaRenderer::transportDescrCB, "/AVT/control",
              &DLNAMediaRenderer::controlCB, "/AVT/event",
              eventSubscriptionHandler);
    avt.subscription_namespace_abbrev = "AVT";

    addService(avt);
  }

  void setupConnectionManagerService(IHttpServer* server) {
    DLNAServiceInfo cm;
    cm.setup("urn:schemas-upnp-org:service:ConnectionManager:1",
             "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
             &DLNAMediaRenderer::connmgrDescrCB, "/CM/control",
             &DLNAMediaRenderer::controlCB, "/CM/event",
             &DLNAMediaRenderer::eventSubscriptionHandler);
    cm.subscription_namespace_abbrev = "CMS";

    addService(cm);
  }

  void setupRenderingControlService(IHttpServer* server) {
    DLNAServiceInfo rc;
    rc.setup("urn:schemas-upnp-org:service:RenderingControl:1",
             "urn:upnp-org:serviceId:RenderingControl", "/RC/service.xml",
             &DLNAMediaRenderer::controlDescrCB, "/RC/control",
             &DLNAMediaRenderer::controlCB, "/RC/event",
             &DLNAMediaRenderer::eventSubscriptionHandler);
    rc.subscription_namespace_abbrev = "RCS";

    addService(rc);
  }

  /// Register all services (called when HTTP server is set)
  void setupServicesImpl(IHttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::setupServices");
    // register the individual services and register their endpoints on the
    // provided HTTP server
    setupTransportService(server);
    setupConnectionManagerService(server);
    setupRenderingControlService(server);
  }

  /**
   * @brief Process parsed SOAP ActionRequest and dispatch to appropriate
   * handler
   * @param action Parsed ActionRequest
   * @param server IHttpServer for reply
   * @return true if action handled, false if not supported
   */
  bool processAction(ActionRequest& action, IHttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNAMediaRenderer::processAction: %s",
                   action.getAction());
    auto& action_str = action.getActionStr();
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
                   action.getAction());
    server.replyError(400, "Invalid Action");
    return false;
  }

  // Per-action handlers (match MediaServer pattern)
  bool processActionPlay(ActionRequest& action, IHttpServer& server) {
    if (event_cb) event_cb(MediaEvent::PLAY, *this);
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          (void)ref;
          return DeviceType::printReplyXML(out, "PlayResponse", "AVTransport");
        },
        200, nullptr, this);
    // setActive(true);
    play();
    return true;
  }

  bool processActionPause(ActionRequest& action, IHttpServer& server) {
    if (event_cb) event_cb(MediaEvent::PAUSE, *this);
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          (void)ref;
          return DeviceType::printReplyXML(out, "PauseResponse", "AVTransport");
        },
        200, nullptr, this);
    setActive(false);
    return true;
  }

  bool processActionStop(ActionRequest& action, IHttpServer& server) {
    // use stop() to ensure state reset and application callback are invoked
    if (event_cb) event_cb(MediaEvent::STOP, *this);
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          (void)ref;
          return DeviceType::printReplyXML(out, "StopResponse", "AVTransport");
        },
        200, nullptr, this);
    stop();
    return true;
  }

  bool processActionGetCurrentTransportActions(ActionRequest& action,
                                               IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          // Stream reply directly: delegate to printReplyXML which will
          // write headers and invoke the inner writer with the same ref.
          return DeviceType::printReplyXML(
              out, "GetCurrentTransportActionsResponse", "AVTransport",
              [](Print& o, void* innerRef) -> size_t {
                size_t written = 0;
                auto self = (DLNAMediaRenderer*)innerRef;
                written += o.print("<Actions>");
                written += o.print(self->getCurrentTransportActions());
                written += o.print("</Actions>");
                return written;
              },
              ref);
        },
        200, nullptr, this);
    return true;
  }

  // ConnectionManager: GetProtocolInfo
  bool processActionGetProtocolInfo(ActionRequest& action,
                                    IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaRenderer*)ref;
          return DeviceType::replyGetProtocolInfo(
              out, self->getSourceProtocols(), self->getSinkProtocols());
        },
        200, nullptr, this);
    return true;
  }

  // ConnectionManager: GetCurrentConnectionIDs
  bool processActionGetCurrentConnectionIDs(ActionRequest& action,
                                            IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaRenderer*)ref;
          return DeviceType::replyGetCurrentConnectionIDs(
              out, self->getConnectionID());
        },
        200, nullptr, this);
    return true;
  }

  // ConnectionManager: GetCurrentConnectionInfo
  bool processActionGetCurrentConnectionInfo(ActionRequest& action,
                                             IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaRenderer*)ref;
          return DeviceType::replyGetCurrentConnectionInfo(
              out, self->getSinkProtocols(), self->getConnectionID(), "Input");
        },
        200, nullptr, this);
    return true;
  }

  // AVTransport: GetDeviceCapabilities
  bool processActionGetDeviceCapabilities(ActionRequest& action,
                                          IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          return DeviceType::printReplyXML(
              out, "GetDeviceCapabilitiesResponse", "AVTransport",
              [](Print& o, void* innerRef) -> size_t {
                auto self = (DLNAMediaRenderer*)innerRef;
                size_t written = 0;
                written += o.print("<PlayMedia>");
                written += o.print(
                    StrView(self->getPossiblePlaybackStorageMedia()).c_str());
                written += o.print("</PlayMedia>");
                written += o.print("<RecMedia>");
                written += o.print(
                    StrView(self->getPossibleRecordStorageMedia()).c_str());
                written += o.print("</RecMedia>");
                written += o.print("<RecQualityModes>");
                written += o.print(
                    StrView(self->getPossibleRecordQualityModes()).c_str());
                written += o.print("</RecQualityModes>");
                return written;
              },
              ref);
        },
        200, nullptr, this);
    return true;
  }

  // AVTransport: GetTransportInfo
  bool processActionGetTransportInfo(ActionRequest& action,
                                     IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          return DeviceType::printReplyXML(
              out, "GetTransportInfoResponse", "AVTransport",
              [](Print& o, void* innerRef) -> size_t {
                auto self = (DLNAMediaRenderer*)innerRef;
                size_t written = 0;
                // CurrentTransportState
                written += o.print("<CurrentTransportState>");
                written += o.print(self->getTransportState());
                written += o.print("</CurrentTransportState>");

                // CurrentTransportStatus - return OK by default
                written += o.print(
                    "<CurrentTransportStatus>OK</CurrentTransportStatus>");

                // CurrentSpeed - simple renderer: report "1"
                written += o.print("<CurrentSpeed>1</CurrentSpeed>");
                return written;
              },
              ref);
        },
        200, nullptr, this);
    return true;
  }

  // AVTransport: GetTransportSettings
  bool processActionGetTransportSettings(ActionRequest& action,
                                         IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          return DeviceType::printReplyXML(
              out, "GetTransportSettingsResponse", "AVTransport",
              [](Print& o, void* innerRef) -> size_t {
                auto self = (DLNAMediaRenderer*)innerRef;
                size_t written = 0;
                // PlayMode: report configured play mode
                written += o.print("<PlayMode>");
                written += o.print(StrView(self->getPlayMode()).c_str());
                written += o.print("</PlayMode>");

                // RecQualityMode: single value - report NOT_IMPLEMENTED by
                // default
                written += o.print("<RecQualityMode>");
                written += o.print("NOT_IMPLEMENTED");
                written += o.print("</RecQualityMode>");
                return written;
              },
              ref);
        },
        200, nullptr, this);
    return true;
  }

  // AVTransport: GetMediaInfo
  bool processActionGetMediaInfo(ActionRequest& action, IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          return DeviceType::printReplyXML(
              out, "GetMediaInfoResponse", "AVTransport",
              [](Print& o, void* innerRef) -> size_t {
                auto self = (DLNAMediaRenderer*)innerRef;
                size_t written = 0;
                // NrTracks: simple heuristic - 1 if a URI is present, 0
                // otherwise
                written += o.print("<NrTracks>");
                written += o.print(
                    self->getCurrentUri() && *self->getCurrentUri() ? 1 : 0);
                written += o.print("</NrTracks>");

                // MediaDuration: unknown here -> empty string
                written += o.print("<MediaDuration></MediaDuration>");

                // CurrentURI and CurrentURIMetaData
                written += o.print("<CurrentURI>");
                written += o.print(StrView(self->getCurrentUri()).c_str());
                written += o.print("</CurrentURI>");
                written += o.print("<CurrentURIMetaData>");
                written +=
                    o.print(StrView(self->getCurrentUriMetadata()).c_str());
                written += o.print("</CurrentURIMetaData>");

                // NextURI and NextURIMetaData: not supported -> empty
                written += o.print("<NextURI>NOT_IMPLEMENTED</NextURI>");
                written += o.print(
                    "<NextURIMetaData>NOT_IMPLEMENTED</NextURIMetaData>");

                // PlayMedium / RecordMedium / WriteStatus: use configured
                // defaults
                written += o.print("<PlayMedium>");
                written += o.print(
                    StrView(self->getPossiblePlaybackStorageMedia()).c_str());
                written += o.print("</PlayMedium>");

                written += o.print("<RecordMedium>");
                written += o.print(
                    StrView(self->getPossibleRecordStorageMedia()).c_str());
                written += o.print("</RecordMedium>");

                written +=
                    o.print("<WriteStatus>NOT_IMPLEMENTED</WriteStatus>");

                return written;
              },
              ref);
        },
        200, nullptr, this);
    return true;
  }

  bool processActionSetAVTransportURI(ActionRequest& action,
                                      IHttpServer& server) {
    const char* uri = action.getArgumentValue("CurrentURI");
    current_uri_metadata = action.getArgumentValue("CurrentURIMetaData");
    if (uri && *uri) {
      setPlaybackURL(uri);
      if (event_cb) event_cb(MediaEvent::SET_URI, *this);
      server.reply(
          "text/xml",
          [](Print& out, void* ref) -> size_t {
            (void)ref;
            return DeviceType::printReplyXML(out, "SetAVTransportURIResponse",
                                             "AVTransport");
          },
          200, nullptr, this);
      return true;
    } else {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "SetAVTransportURI called with invalid SOAP payload");
      server.replyError(400, "Invalid Action");
      return false;
    }
  }

  bool processActionSetVolume(ActionRequest& action, IHttpServer& server) {
    int desiredVolume = action.getArgumentIntValue("DesiredVolume");
    setVolume((uint8_t)desiredVolume);
    if (event_cb) event_cb(MediaEvent::SET_VOLUME, *this);
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          (void)ref;
          return DeviceType::printReplyXML(out, "SetVolumeResponse",
                                           "RenderingControl");
        },
        200, nullptr, this);
    return true;
  }

  bool processActionSetMute(ActionRequest& action, IHttpServer& server) {
    bool desiredMute = action.getArgumentIntValue("DesiredMute") != 0;
    setMuted(desiredMute);
    if (event_cb) event_cb(MediaEvent::SET_MUTE, *this);
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          (void)ref;
          return DeviceType::printReplyXML(out, "SetMuteResponse",
                                           "RenderingControl");
        },
        200, nullptr, this);
    return true;
  }

  bool processActionGetMute(ActionRequest& action, IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          return DeviceType::printReplyXML(
              out, "GetMuteResponse", "RenderingControl",
              [](Print& o, void* innerRef) -> size_t {
                auto self = (DLNAMediaRenderer*)innerRef;
                size_t written = 0;
                written += o.print("<CurrentMute>");
                written += o.print(self->isMuted() ? 1 : 0);
                written += o.print("</CurrentMute>");
                return written;
              },
              ref);
        },
        200, nullptr, this);
    return true;
  }

  bool processActionGetVolume(ActionRequest& action, IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          return DeviceType::printReplyXML(
              out, "GetVolumeResponse", "RenderingControl",
              [](Print& o, void* innerRef) -> size_t {
                auto self = (DLNAMediaRenderer*)innerRef;
                DlnaLogger.log(DlnaLogLevel::Debug, "GetVolume: %d",
                               self->getVolume());
                size_t written = 0;
                written += o.print("<CurrentVolume>");
                written += o.print((int)self->getVolume());
                written += o.print("</CurrentVolume>");
                return written;
              },
              ref);
        },
        200, nullptr, this);
    return true;
  }

  /// Setup the action handling rules
  void setupRules() {
    rules.push_back({"Play", [](DLNAMediaRenderer* self, ActionRequest& action,
                                IHttpServer& server) {
                       return self->processActionPlay(action, server);
                     }});
    rules.push_back({"Pause", [](DLNAMediaRenderer* self, ActionRequest& action,
                                 IHttpServer& server) {
                       return self->processActionPause(action, server);
                     }});
    rules.push_back({"Stop", [](DLNAMediaRenderer* self, ActionRequest& action,
                                IHttpServer& server) {
                       return self->processActionStop(action, server);
                     }});
    rules.push_back({"GetCurrentTransportActions",
                     [](DLNAMediaRenderer* self, ActionRequest& action,
                        IHttpServer& server) {
                       return self->processActionGetCurrentTransportActions(
                           action, server);
                     }});
    rules.push_back(
        {"GetTransportInfo", [](DLNAMediaRenderer* self, ActionRequest& action,
                                IHttpServer& server) {
           return self->processActionGetTransportInfo(action, server);
         }});
    rules.push_back({"GetTransportSettings",
                     [](DLNAMediaRenderer* self, ActionRequest& action,
                        IHttpServer& server) {
                       return self->processActionGetTransportSettings(action,
                                                                      server);
                     }});
    rules.push_back(
        {"GetMediaInfo", [](DLNAMediaRenderer* self, ActionRequest& action,
                            IHttpServer& server) {
           return self->processActionGetMediaInfo(action, server);
         }});
    rules.push_back({"GetDeviceCapabilities",
                     [](DLNAMediaRenderer* self, ActionRequest& action,
                        IHttpServer& server) {
                       return self->processActionGetDeviceCapabilities(action,
                                                                       server);
                     }});
    rules.push_back(
        {"SetAVTransportURI", [](DLNAMediaRenderer* self, ActionRequest& action,
                                 IHttpServer& server) {
           return self->processActionSetAVTransportURI(action, server);
         }});
    rules.push_back(
        {"SetVolume", [](DLNAMediaRenderer* self, ActionRequest& action,
                         IHttpServer& server) {
           return self->processActionSetVolume(action, server);
         }});
    rules.push_back({"SetMute", [](DLNAMediaRenderer* self,
                                   ActionRequest& action, IHttpServer& server) {
                       return self->processActionSetMute(action, server);
                     }});
    rules.push_back({"GetMute", [](DLNAMediaRenderer* self,
                                   ActionRequest& action, IHttpServer& server) {
                       return self->processActionGetMute(action, server);
                     }});
    rules.push_back(
        {"GetVolume", [](DLNAMediaRenderer* self, ActionRequest& action,
                         IHttpServer& server) {
           return self->processActionGetVolume(action, server);
         }});
    // ConnectionManager rules
    rules.push_back(
        {"GetProtocolInfo", [](DLNAMediaRenderer* self, ActionRequest& action,
                               IHttpServer& server) {
           return self->processActionGetProtocolInfo(action, server);
         }});
    rules.push_back({"GetCurrentConnectionIDs",
                     [](DLNAMediaRenderer* self, ActionRequest& action,
                        IHttpServer& server) {
                       return self->processActionGetCurrentConnectionIDs(
                           action, server);
                     }});
    rules.push_back({"GetCurrentConnectionInfo",
                     [](DLNAMediaRenderer* self, ActionRequest& action,
                        IHttpServer& server) {
                       return self->processActionGetCurrentConnectionInfo(
                           action, server);
                     }});
  }
};

}  // namespace tiny_dlna
