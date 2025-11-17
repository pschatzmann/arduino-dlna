// Minimal header-only Digital Media Server (MediaServer) device
#pragma once

#include "DLNAMediaServerDescr.h"
#include "MediaItem.h"
#include "basic/EscapingPrint.h"
#include "basic/Printf.h"
#include "basic/Str.h"
#include "dlna/Action.h"
#include "dlna/devices/DLNADevice.h"
#include "dlna/devices/MediaServer/MediaItem.h"
#include "dlna/udp/IUDPService.h"
#include "dlna/xml/XMLParserPrint.h"
#include "dlna/xml/XMLPrinter.h"
#include "http/Http.h"
#include "http/Server/IHttpServer.h"

namespace tiny_dlna {

/**
 * @class DLNAMediaServer
 * @brief Digital Media Server implementation
 *
 * Lightweight DLNA MediaServer with ContentDirectory (Browse/Search)
 * and ConnectionManager services. Register PrepareDataCallback and
 * GetDataCallback and optionally setReference(void*) for custom context.
 *
 * This class provides a complete DLNA Digital Media Server (DMS) implementation
 * supporting:
 * - ContentDirectory service (Browse, Search, GetSearchCapabilities, etc.)
 * - ConnectionManager service (GetProtocolInfo, GetCurrentConnectionIDs, etc.)
 * - UPnP event subscriptions for state change notifications
 *
 * Usage:
 * 1. Create an instance with HttpServer and IUDPService
 * 2. Set callbacks for content preparation and retrieval
 * 3. Call begin() to start the server
 * 4. Call loop() repeatedly to process requests
 *
 * @tparam ClientType Arduino `Client` implementation used for outbound HTTP
 *         control and event traffic (e.g. `WiFiClient`, `EthernetClient`).
 *
 * Author: Phil Schatzmann
 */
template <typename ClientType>
class DLNAMediaServer : public DLNADeviceInfo {
 public:
  using HttpClient = ClientType;
  using DeviceType = DLNADevice<ClientType>;
  /// Callback: prepare data for Browse/Search. Fills numberReturned,
  /// totalMatches and updateID. Last parameter is user reference pointer.
  typedef void (*PrepareDataCallback)(
      const char* objectID, ContentQueryType queryType, const char* filter,
      int startingIndex, int requestedCount, const char* sortCriteria,
      int& numberReturned, int& totalMatches, int& updateID, void* reference);

  /// Callback: retrieve MediaItem by index. Returns true if item is valid.
  typedef bool (*GetDataCallback)(int index, MediaItem& item, void* reference);
  /// Alternative callback: directly print DIDL entry for given index to out.
  /// Returns number of bytes written; return 0 to stop iteration early.
  typedef size_t (*GetDataCallbackPrint)(int index, Print& out,
                                         void* reference);

  /// Default constructor: initialize device info and defaults
  DLNAMediaServer() {
    setSerialNumber(usn);
    setDeviceType(st);
    setFriendlyName("ArduinoMediaServer");
    setManufacturer("TinyDLNA");
    setManufacturerURL("https://github.com/pschatzmann/arduino-dlna");
    setModelName("TinyDLNA MediaServer");
    setModelNumber("1.0");
    setBaseURL("http://localhost:44757");
    setupRules();
  }
  /// Construct MediaServer with an HttpServer and IUDPService pre-set
  DLNAMediaServer(IHttpServer& server, IUDPService& udp) : DLNAMediaServer() {
    // use setters so derived classes or overrides get a consistent path
    setHttpServer(server);
    setUdpService(udp);
    dlna_device.setReference(this);
  }

  /// Destructor
  ~DLNAMediaServer() { end(); }

  /// Set the http server instance the MediaServer should use
  void setHttpServer(IHttpServer& server) {
    p_server = &server;
    setupServicesImpl(&server);
  }

  /// Set the UDP service instance the MediaServer should use
  void setUdpService(IUDPService& udp) { p_udp_member = &udp; }

  /// Start the underlying DLNA device using previously provided UDP and HTTP
  /// server. Call this after constructing MediaServer(HttpServer&,
  /// IUDPService&)
  bool begin() {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::begin");
    return dlna_device.begin(*this, *p_udp_member, *p_server);
  }

  /// Stops the processing and releases the resources
  void end() {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::end");
    dlna_device.end();
  }

  /// call this method in the Arduino loop as often as possible
  bool loop(int loopAction = RUN_ALL) { return dlna_device.loop(loopAction); }

  /// Define the search capabilities: use csv
  void setSearchCapabilities(const char* caps) { g_search_capabiities = caps; }

  /// Get the search capabilities string (CSV)
  const char* getSearchCapabilities() { return g_search_capabiities; }

  /// Define the sort capabilities: use csv
  void setSortCapabilities(const char* caps) { g_sort_capabilities = caps; }

  /// Get the sort capabilities string (CSV)
  const char* getSortCapabilities() { return g_sort_capabilities; }

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
      auto self = (DLNAMediaServer*)ref;
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

  /// Sets the callback that prepares the data for the Browse and Search
  void setPrepareDataCallback(PrepareDataCallback cb) { prepare_data_cb = cb; }

  /// Sets the data callback that provides a MediaItem by index
  void setGetDataCallback(GetDataCallback cb) { get_data_cb = cb; }
  /// Sets the alternative data callback that prints a DIDL entry directly
  void setGetDataCallback(GetDataCallbackPrint cb) { get_data_print_cb = cb; }

  /// Sets a user reference pointer, available in callbacks
  void setReference(void* ref) { reference_ = ref; }

  /// Provides access to the http server
  IHttpServer* getHttpServer() { return p_server; }

  /// Provides access to the system update ID
  int getSystemUpdateID() { return g_stream_updateID; }

  /// Call this method if content has changed : Increments and returns the
  /// SystemUpdateID
  int incrementSystemUpdateID() { return ++g_stream_updateID; }

  /// Provides access to the internal DLNA device instance
  IDevice& device() { return dlna_device; }

  /// Enable/disable subscription notifications
  void setSubscriptionsActive(bool flag) {
    dlna_device.setSubscriptionsActive(flag);
  }

  /// Query whether subscription notifications are active
  bool isSubscriptionsActive() { return dlna_device.isSubscriptionsActive(); }

  /// Define your own custom logic
  void setCustomActionRule(const char* suffix,
                           bool (*handler)(DLNAMediaServer*, ActionRequest&,
                                           IHttpServer&)) {
    for (size_t i = 0; i < rules.size(); ++i) {
      if (StrView(rules[i].suffix).equals(suffix)) {
        rules[i].handler = handler;
        return;
      }
    }
    rules.push_back({suffix, handler});
  }

  /// Set a custom ContentDirectory SCPD descriptor (per-instance)
  void setContentDirectoryDescr(DLNADescr& d) { p_contentDirectoryDescr = &d; }

  /// Get pointer to the per-instance ContentDirectory descriptor
  DLNADescr& getContentDirectoryDescr() { return *p_contentDirectoryDescr; }

  /// Set a custom ConnectionManager SCPD descriptor (per-instance)
  void setConnectionMgrDescr(DLNADescr& d) { p_connmgrDescr = &d; }

  /// Get pointer to the per-instance ConnectionManager descriptor
  DLNADescr& getConnectionMgrDescr() { return *p_connmgrDescr; }

  /// Log current status of subscriptions and scheduler
  void logStatus() { return dlna_device.logStatus(); }

 protected:
  /// Individual action rule for handling specific actions
  struct ActionRule {
    const char* suffix;
    bool (*handler)(DLNAMediaServer*, ActionRequest&, IHttpServer&);
  };

  DeviceType dlna_device;
  PrepareDataCallback prepare_data_cb = nullptr;
  GetDataCallback get_data_cb = nullptr;
  GetDataCallbackPrint get_data_print_cb = nullptr;
  IHttpServer* p_server = nullptr;
  IUDPService* p_udp_member = nullptr;
  void* reference_ = nullptr;
  GetDataCallback g_stream_get_data_cb = nullptr;
  int g_stream_numberReturned = 0;
  int g_stream_totalMatches = 0;
  int g_stream_updateID = 1;
  int g_stream_startingIndex = 0;
  void* g_stream_reference = nullptr;
  const char* st = "urn:schemas-upnp-org:device:MediaServer:1";
  const char* usn = "uuid:media-server-0000-0000-0000-000000000001";
  const char* g_search_capabiities =
      "dc:title,dc:creator,upnp:class,upnp:genre,"
      "upnp:album,upnp:artist,upnp:albumArtURI";
  const char* g_sort_capabilities =
      "dc:title,dc:date,upnp:class,upnp:album,upnp:episodeNumber,upnp:"
      "originalTrackNumber";
  // Default protocol info values (use DLNA_PROTOCOL by default)
  const char* sourceProto = DLNA_PROTOCOL_AUDIO;
  const char* sinkProto = "";
  const char* connectionID = "0";
  Vector<ActionRule> rules;
  // Per-instance descriptor objects (default-initialized). These allow callers
  // to customize the SCPD delivered for this MediaServer instance.
  DLNAMediaServerContentDirectoryDescr default_contentDirectoryDescr;
  DLNADescr* p_contentDirectoryDescr = &default_contentDirectoryDescr;
  DLNAMediaServerConnectionMgrDescr default_connectionMgrDescr;
  DLNADescr* p_connmgrDescr = &default_connectionMgrDescr;

  /// serviceAbbrev: e.g. "AVT" or subscription namespace abbrev defined for
  /// the service
  void addChange(const char* serviceAbbrev,
                 std::function<size_t(Print&, void*)> changeWriter) {
    dlna_device.addChange(serviceAbbrev, changeWriter, this);
  }

  /// Publish a ContentDirectory event (SystemUpdateID)
  void publishAVT() {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::publishAVT");
    // Publish SystemUpdateID using a writer that reads the current value
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaServer*)ref;
      size_t result = 0;
      result += out.print("<SystemUpdateID val=\"");
      result += out.print(self->g_stream_updateID);
      result += out.println("\"/>");
      return result;
    };
    addChange("AVT", writer);
  }

  /// Publish a ConnectionManager event (CurrentConnectionIDs)
  void publishCMS() {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::publishCMS");
    // Publish current ConnectionManager properties using a writer that
    // reads live state from the device
    auto writer = [](Print& out, void* ref) -> size_t {
      auto self = (DLNAMediaServer*)ref;
      size_t result = 0;
      result += out.print("<SourceProtocolInfo val=\"");
      result += out.print(StrView(self->getSourceProtocols()).c_str());
      result += out.println("\"/>");
      result += out.print("<SinkProtocolInfo val=\"");
      result += out.print(StrView(self->getSinkProtocols()).c_str());
      result += out.println("\"/>");
      result += out.print("<CurrentConnectionIDs val=\"");
      result += out.print(StrView(self->connectionID).c_str());
      result += out.println("\"/>");
      return result;
    };
    addChange("CMS", writer);
  }

  /// Setup the service endpoints
  void setupServicesImpl(IHttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::setupServices");

    // register the individual services via helpers
    setupContentDirectoryService(server);
    setupConnectionManagerService(server);
  }

  /// Static descriptor callback for ContentDirectory SCPD
  static void contentDescCB(IHttpServer* server, const char* requestPath,
                            HttpRequestHandlerLine* hl) {
    DLNAMediaServer* self = getMediaServer(server);
    assert(self != nullptr);
    // Non-capturing lambda matches function-pointer signature and can be
    // passed directly to reply (avoids extra static helpers).
    server->reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          size_t result = 0;
          if (ref) {
            auto self = (DLNAMediaServer*)ref;
            result += self->getContentDirectoryDescr().printDescr(out);
          } else {
            tiny_dlna::DLNAMediaServerContentDirectoryDescr descr;
            result += descr.printDescr(out);
          }
          return result;
        },
        200, nullptr, self);
  }

  /// Static descriptor callback for ConnectionManager SCPD
  static void connDescCB(IHttpServer* server, const char* requestPath,
                         HttpRequestHandlerLine* hl) {
    DLNAMediaServer* self = getMediaServer(server);

    server->reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          size_t result = 0;
          if (ref) {
            auto self = (DLNAMediaServer*)ref;
            result += self->getConnectionMgrDescr().printDescr(out);
          } else {
            tiny_dlna::DLNAMediaServerConnectionMgrDescr descr;
            result += descr.printDescr(out);
          }
          return result;
        },
        200, nullptr, self);
  }

  // (Removed static helper functions — lambdas are used inline at callsite.)

  /// After the subscription we publish all relevant properties
  static void eventSubscriptionHandler(IHttpServer* server,
                                       const char* requestPath,
                                       HttpRequestHandlerLine* hl) {
    bool is_subscribe = false;
    DeviceType::handleSubscription(server, requestPath, hl, is_subscribe);
    if (is_subscribe) {
      DLNAMediaServer* self = getMediaServer(server);
      assert(self != nullptr);
      StrView request_path_str(requestPath);
      if (request_path_str.contains("/CD/"))
        self->publishAVT();
      else if (request_path_str.contains("/CM/"))
        self->publishCMS();
      else
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "eventSubscriptionHandler: Unknown request path: %s",
                       requestPath);
    }
  }

  /// Setup and register ContentDirectory service
  void setupContentDirectoryService(IHttpServer* server) {
    DLNAServiceInfo cd;
    cd.setup("urn:schemas-upnp-org:service:ContentDirectory:1",
             "urn:upnp-org:serviceId:ContentDirectory", "/CD/service.xml",
             &DLNAMediaServer::contentDescCB, "/CD/control",
             &DLNAMediaServer::contentDirectoryControlCB, "/CD/event",
             [](IHttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });

    // subscription namespace abbreviation used for event publishing
    cd.subscription_namespace_abbrev = "AVT";

    addService(cd);
  }

  /// Setup and register ConnectionManager service
  void setupConnectionManagerService(IHttpServer* server) {
    DLNAServiceInfo cm;
    cm.setup("urn:schemas-upnp-org:service:ConnectionManager:1",
             "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
             &DLNAMediaServer::connDescCB, "/CM/control",
             &DLNAMediaServer::connmgrControlCB, "/CM/event",
             &DLNAMediaServer::eventSubscriptionHandler);

    // subscription namespace abbreviation used for event publishing
    cm.subscription_namespace_abbrev = "CMS";

    addService(cm);
  }

  /// Process action requests using rules-based dispatch
  bool processAction(ActionRequest& action, IHttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNAMediaServer::processAction: %s",
                   action.getAction());
    auto& action_str = action.getActionStr();
    if (action_str.isEmpty()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Empty action received");
      server.replyNotFound();
      return false;
    }
    for (const auto& rule : rules) {
      if (action_str.endsWith(rule.suffix)) {
        return rule.handler(this, action, server);
      }
    }
    DlnaLogger.log(DlnaLogLevel::Error, "Unsupported action: %s",
                   action.getAction());
    server.replyNotFound();
    return false;
  }

  /// Handle ContentDirectory:Browse action
  bool processActionBrowse(ActionRequest& action, IHttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "processActionBrowse");
    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = g_stream_updateID;

    // extract paging args
    int startingIndex = action.getArgumentIntValue("StartingIndex");
    int requestedCount = action.getArgumentIntValue("RequestedCount");

    // determine query type (BrowseDirectChildren / BrowseMetadata / Search)
    ContentQueryType qtype =
        parseContentQueryType(action.getArgumentValue("BrowseFlag"));
    // query for data
    if (prepare_data_cb) {
      prepare_data_cb(action.getArgumentValue("ObjectID"), qtype,
                      action.getArgumentValue("Filter"), startingIndex,
                      requestedCount, action.getArgumentValue("SortCriteria"),
                      numberReturned, totalMatches, updateID, reference_);
    }

    // Store streaming state and reply using callback writer
    g_stream_numberReturned = numberReturned;
    g_stream_totalMatches = totalMatches;
    g_stream_updateID = updateID;
    g_stream_startingIndex = startingIndex;

    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaServer*)ref;
          return self->streamActionItems(out, "BrowseResponse",
                                         self->g_stream_startingIndex);
        },
        200, nullptr, this);
    return true;
  }

  /// Handle ContentDirectory:Search action
  bool processActionSearch(ActionRequest& action, IHttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "processActionSearch");
    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = g_stream_updateID;

    // extract paging args
    int startingIndex = action.getArgumentIntValue("StartingIndex");
    int requestedCount = action.getArgumentIntValue("RequestedCount");

    // For Search actions we always use the Search query type regardless of
    // any provided BrowseFlag; this ensures Search semantics are preserved.
    ContentQueryType qtype = ContentQueryType::Search;
    // query for data
    if (prepare_data_cb) {
      prepare_data_cb(action.getArgumentValue("ContainerID"), qtype,
                      action.getArgumentValue("Filter"), startingIndex,
                      requestedCount, action.getArgumentValue("SortCriteria"),
                      numberReturned, totalMatches, updateID, reference_);
    }

    // Store streaming state and reply using callback writer
    g_stream_numberReturned = numberReturned;
    g_stream_totalMatches = totalMatches;
    g_stream_updateID = updateID;
    g_stream_startingIndex = startingIndex;

    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaServer*)ref;
          return self->streamActionItems(out, "SearchResponse",
                                         self->g_stream_startingIndex);
        },
        200, nullptr, this);
    return true;
  }

  /// Handle ContentDirectory:GetSearchCapabilities action
  bool processActionGetSearchCapabilities(ActionRequest& action,
                                          IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaServer*)ref;
          size_t written = 0;
          written += self->soapEnvelopeStart(out);
          written += self->actionResponseStart(
              out, "GetSearchCapabilitiesResponse",
              "urn:schemas-upnp-org:service:ContentDirectory:1");
          written += out.print("<SearchCaps>");
          written += out.print(StrView(self->g_search_capabiities).c_str());
          written += out.print("</SearchCaps>\r\n");
          written +=
              self->actionResponseEnd(out, "GetSearchCapabilitiesResponse");
          written += self->soapEnvelopeEnd(out);
          return written;
        },
        200, nullptr, this);
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processActionGetSearchCapabilities (callback)");
    return true;
  }

  /// Handle ContentDirectory:GetSortCapabilities action
  bool processActionGetSortCapabilities(ActionRequest& action,
                                        IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaServer*)ref;
          size_t written = 0;
          written += self->soapEnvelopeStart(out);
          written += self->actionResponseStart(
              out, "GetSortCapabilitiesResponse",
              "urn:schemas-upnp-org:service:ContentDirectory:1");
          written += out.print("<SortCaps>");
          written += out.print(StrView(self->g_sort_capabilities).c_str());
          written += out.print("</SortCaps>\r\n");
          written +=
              self->actionResponseEnd(out, "GetSortCapabilitiesResponse");
          written += self->soapEnvelopeEnd(out);
          return written;
        },
        200, nullptr, this);
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processActionGetSortCapabilities (callback)");
    return true;
  }

  /// Handle ContentDirectory:GetSystemUpdateID action
  bool processActionGetSystemUpdateID(ActionRequest& action,
                                      IHttpServer& server) {
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = (DLNAMediaServer*)ref;
          size_t written = 0;
          written += self->soapEnvelopeStart(out);
          written += self->actionResponseStart(
              out, "GetSystemUpdateIDResponse",
              "urn:schemas-upnp-org:service:ContentDirectory:1");
          Printf pr{out};
          written += static_cast<size_t>(
              pr.printf("<Id>%d</Id>\r\n", self->g_stream_updateID));
          written += self->actionResponseEnd(out, "GetSystemUpdateIDResponse");
          written += self->soapEnvelopeEnd(out);
          return written;
        },
        200, nullptr, this);
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processActionGetSystemUpdateID (callback): %d",
                   g_stream_updateID);
    return true;
  }

  /// Replies with Source and Sink protocol lists (CSV protocolInfo strings)
  bool processActionGetProtocolInfo(ActionRequest& action,
                                    IHttpServer& server) {
    // Stream reply directly using a writer callback to avoid temporary buffers
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = static_cast<DLNAMediaServer*>(ref);
          return DeviceType::replyGetProtocolInfo(out, self->sourceProto,
                                                  self->sinkProto);
        },
        200, nullptr, this);
    return true;
  }

  /// Handle ConnectionManager:GetCurrentConnectionIDs action
  bool processActionGetCurrentConnectionIDs(ActionRequest& action,
                                            IHttpServer& server) {
    // Stream reply directly using a writer callback
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = static_cast<DLNAMediaServer*>(ref);
          return DeviceType::replyGetCurrentConnectionIDs(out,
                                                          self->connectionID);
        },
        200, nullptr, this);
    return true;
  }
  /// Handle ConnectionManager:GetCurrentConnectionInfo action
  bool processActionGetCurrentConnectionInfo(ActionRequest& action,
                                             IHttpServer& server) {
    // Read requested ConnectionID (not used in this simple implementation)
    int connId = action.getArgumentIntValue("ConnectionID");

    // Stream the GetCurrentConnectionInfo response using chunked encoding
    // to avoid building the full response in memory and to remain
    // consistent with other handlers.
    server.reply(
        "text/xml",
        [](Print& out, void* ref) -> size_t {
          auto self = static_cast<DLNAMediaServer*>(ref);
          return DeviceType::replyGetCurrentConnectionInfo(
              out, self->sourceProto, self->connectionID, "Output");
        },
        200, nullptr, this);
    return true;
  }

  /// Common helper to stream a ContentDirectory response (Browse or Search)
  ContentQueryType parseContentQueryType(const char* flag) {
    StrView fv(flag);
    if (fv.equals("BrowseDirectChildren"))
      return ContentQueryType::BrowseChildren;
    if (fv.equals("BrowseMetadata")) return ContentQueryType::BrowseMetadata;
    // fallback
    return ContentQueryType::BrowseMetadata;
  }

  size_t soapEnvelopeStart(Print& out) {
    size_t written = 0;
    written += out.print("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
    written += out.print(
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" ");
    written += out.print(
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n");
    written += out.print("<s:Body>\r\n");
    return written;
  }

  size_t soapEnvelopeEnd(Print& out) {
    size_t written = 0;
    written += out.print("</s:Body>\r\n");
    written += out.print("</s:Envelope>\r\n");
    return written;
  }

  size_t actionResponseStart(Print& out, const char* responseName,
                             const char* serviceNS) {
    // writes: <u:ResponseName xmlns:u="serviceNS">
    Printf pr{out};
    return static_cast<size_t>(
        pr.printf("<u:%s xmlns:u=\"%s\">\r\n", responseName, serviceNS));
  }

  size_t actionResponseEnd(Print& out, const char* responseName) {
    Printf pr{out};
    return static_cast<size_t>(pr.printf("</u:%s>\r\n", responseName));
  }

  size_t streamActionItems(Print& out, const char* responseName,
                           int startingIndex) {
    // SOAP envelope start and response wrapper
    size_t written = 0;
    written += soapEnvelopeStart(out);
    written += actionResponseStart(
        out, responseName, "urn:schemas-upnp-org:service:ContentDirectory:1");

    // Result -> DIDL-Lite
    written += out.println("<Result>");
    // DIDL root with namespaces
    written += streamDIDL(out, g_stream_numberReturned, startingIndex);
    written += out.print("</Result>\r\n");

    // numeric fields
    Printf pr{out};
    written += static_cast<size_t>(pr.printf(
        "<NumberReturned>%d</NumberReturned>\r\n", g_stream_numberReturned));
    written += static_cast<size_t>(pr.printf(
        "<TotalMatches>%d</TotalMatches>\r\n", g_stream_totalMatches));
    written += static_cast<size_t>(
        pr.printf("<UpdateID>%d</UpdateID>\r\n", g_stream_updateID));

    written += actionResponseEnd(out, responseName);
    written += soapEnvelopeEnd(out);
    return written;
  }

  /// Stream DIDL-Lite payload for a Browse/Search result
  size_t streamDIDL(Print& out, int numberReturned, int startingIndex) {
    size_t written = 0;
    // Stream DIDL-Lite Result payload using helper
    written += out.print(
        "&lt;DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
        "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\"&gt;\r\n");

    written += streamDIDLItems(out, numberReturned, startingIndex);

    written += out.print("&lt;/DIDL-Lite&gt;\r\n");
    return written;
  }

  // Streams each MediaItem entry (container/item with title,
  // class and optional resource). Returns bytes written.
  // Single-pass loop that prefers get_data_print_cb and falls back
  // to get_data_cb for any item where print produced 0 bytes.
  size_t streamDIDLItems(Print& out, int numberReturned, int startingIndex) {
    if (get_data_print_cb == nullptr && get_data_cb == nullptr) {
      // no data callback defined
      return 0;
    }
    EscapingPrint esc_out(out);
    size_t total = 0;
    for (int i = 0; i < numberReturned; ++i) {
      int idx = startingIndex + i;

      // 1) Try print callback if present
      if (get_data_print_cb) {
        size_t w = get_data_print_cb(idx, esc_out, reference_);
        if (w > 0) {
          total += w;
          continue;  // done with this item
        }
      }

      // 2) Structured callback path
      if (get_data_cb) {
        MediaItem item;
        if (!get_data_cb(idx, item, reference_)) break;  // end-of-list
        total += streamDIDLItem(esc_out, item);
        continue;
      }
    }
    return total;
  }

  // Print a single MediaItem as escaped DIDL (returns bytes written)
  size_t streamDIDLItem(Print& out, const MediaItem& item) {
    size_t written = 0;
    const char* mediaItemClassStr = toStr(item.itemClass);
    const char* nodeName =
        (item.itemClass == MediaItemClass::Folder) ? "container" : "item";
    Printf pr{out};
    written += static_cast<size_t>(pr.printf(
        "<%s id=\"%s\" parentID=\"%s\" restricted=\"%d\">", nodeName,
        item.id ? item.id : "", item.parentID ? item.parentID : "0",
        item.restricted ? 1 : 0));

    written += out.print("<dc:title>");
    written += out.print(StrView(item.title).c_str());
    written += out.print("</dc:title>\r\n");
    if (mediaItemClassStr != nullptr) {
      written += static_cast<size_t>(pr.printf(
          "<upnp:class>%s</upnp:class>\r\n", mediaItemClassStr));
    }

    // Optional album art URI
    if (!StrView(item.albumArtURI).isEmpty()) {
      written += out.print("<upnp:albumArtURI>");
      written += out.print(getUri(item.albumArtURI));
      written += out.print("</upnp:albumArtURI>\r\n");
    }

    // res with optional protocolInfo attribute
    if (!StrView(item.resourceURI).isEmpty()) {
      if (!StrView(item.mimeType).isEmpty()) {
        written += static_cast<size_t>(pr.printf(
            "<res protocolInfo=\"http-get:*:%s:*\">", item.mimeType));
        written += out.print(getUri(item.resourceURI));
        written += out.print("</res>\r\n");
      } else {
        written += out.print("<res>");
        written += out.print(getUri(item.resourceURI));
        written += out.print("</res>\r\n");
      }
    }

    written += static_cast<size_t>(pr.printf("</%s>\r\n", nodeName));
    return written;
  }

  const char* getUri(const char* path){
    static Str url{256};
    if (path == nullptr)
      path = "";
    if (!StrView(path).startsWith("http://")) {
      url = getBaseURL();
      if (!StrView(path).startsWith("/")) {
        url += "/";
      }
      url += path;
      return url.c_str();
    }
    return path;
  }

  // Removed separate streamDIDLItemsGetData helper; unified into
  // streamDIDLItems

  const char* toStr(MediaItemClass itemClass) {
    switch (itemClass) {
      case MediaItemClass::Music:
        return "object.item.audioItem.musicTrack";
      case MediaItemClass::Radio:
        return "object.item.audioItem.audioBroadcast";
      case MediaItemClass::Video:
        return "object.item.videoItem.movie";
      case MediaItemClass::Photo:
        return "object.item.imageItem.photo";
      case MediaItemClass::Folder:
        return "object.container";
      default:
        return nullptr;
    }
  }

  /// Control handler for ContentDirectory service
  static void contentDirectoryControlCB(IHttpServer* server,
                                        const char* requestPath,
                                        HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DeviceType::parseActionRequest(server, requestPath, hl, action);

    // If a user-provided callback is registered, hand over the parsed
    // ActionRequest for custom handling. The callback is responsible for
    // writing the HTTP reply if it handles the action.
    DLNAMediaServer* ms = getMediaServer(server);

    // process the requested action using instance method if available
    if (ms) {
      if (ms->processAction(action, *server)) return;
    }

    server->replyNotFound();
  }

  /**
   * @brief Helper to retrieve DLNAMediaServer instance from HttpServer
   * reference chain
   *
   * Navigates through the server's reference (DLNADevice) to get the
   * MediaServer instance.
   *
   * @param server Pointer to the HttpServer instance
   * @return Pointer to the DLNAMediaServer instance, or nullptr if not found
   */
  static DLNAMediaServer* getMediaServer(IHttpServer* server) {
    if (!server) return nullptr;
    auto* dev = static_cast<IDevice*>(server->getReference());
    if (!dev) return nullptr;
    return static_cast<DLNAMediaServer*>(dev->getReference());
  }

  /// simple connection manager control that replies OK
  static void connmgrControlCB(IHttpServer* server, const char* requestPath,
                               HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DeviceType::parseActionRequest(server, requestPath, hl, action);

    DLNAMediaServer* ms = getMediaServer(server);
    if (!ms) {
      server->replyNotFound();
      return;
    }

    // Use rules-based dispatch for ConnectionManager actions
    if (ms->processAction(action, *server)) return;
    server->replyNotFound();
  }

  /// Setup the action rules for supported ContentDirectory and
  /// ConnectionManager commands
  void setupRules() {
    // ContentDirectory rules
    rules.push_back({"Browse", [](DLNAMediaServer* self, ActionRequest& action,
                                  IHttpServer& server) {
                       return self->processActionBrowse(action, server);
                     }});
    rules.push_back({"Search", [](DLNAMediaServer* self, ActionRequest& action,
                                  IHttpServer& server) {
                       return self->processActionSearch(action, server);
                     }});
    rules.push_back(
        {"GetSearchCapabilities",
         [](DLNAMediaServer* self, ActionRequest& action, IHttpServer& server) {
           return self->processActionGetSearchCapabilities(action, server);
         }});
    rules.push_back(
        {"GetSortCapabilities",
         [](DLNAMediaServer* self, ActionRequest& action, IHttpServer& server) {
           return self->processActionGetSortCapabilities(action, server);
         }});
    rules.push_back(
        {"GetSystemUpdateID",
         [](DLNAMediaServer* self, ActionRequest& action, IHttpServer& server) {
           return self->processActionGetSystemUpdateID(action, server);
         }});
    // ConnectionManager rules
    rules.push_back(
        {"GetProtocolInfo",
         [](DLNAMediaServer* self, ActionRequest& action, IHttpServer& server) {
           return self->processActionGetProtocolInfo(action, server);
         }});
    rules.push_back(
        {"GetCurrentConnectionIDs",
         [](DLNAMediaServer* self, ActionRequest& action, IHttpServer& server) {
           return self->processActionGetCurrentConnectionIDs(action, server);
         }});
    rules.push_back(
        {"GetCurrentConnectionInfo",
         [](DLNAMediaServer* self, ActionRequest& action, IHttpServer& server) {
           return self->processActionGetCurrentConnectionInfo(action, server);
         }});
  }
};

}  // namespace tiny_dlna
