// Minimal header-only Digital Media Server (MediaServer) device
#pragma once

#include "DLNAMediaServerDescr.h"
#include "MediaItem.h"
#include "basic/EscapingPrint.h"
#include "basic/Str.h"
#include "dlna/Action.h"
#include "dlna/StringRegistry.h"
#include "dlna/devices/DLNADevice.h"
#include "dlna/devices/MediaServer/MediaItem.h"
#include "dlna/xml/XMLParserPrint.h"
#include "dlna/xml/XMLPrinter.h"
#include "http/Http.h"

namespace tiny_dlna {

/// Digital Media Server implementation
///
/// Lightweight DLNA MediaServer with ContentDirectory (Browse/Search)
/// and ConnectionManager services. Register PrepareDataCallback and
/// GetDataCallback and optionally setReference(void*) for custom context.
class DLNAMediaServer : public DLNADeviceInfo {
 public:
  /// Callback: prepare data for Browse/Search. Fills numberReturned,
  /// totalMatches and updateID. Last parameter is user reference pointer.
  typedef void (*PrepareDataCallback)(
      const char* objectID, ContentQueryType queryType, const char* filter,
      int startingIndex, int requestedCount, const char* sortCriteria,
      int& numberReturned, int& totalMatches, int& updateID, void* reference);

  /// Callback: retrieve MediaItem by index. Returns true if item is valid.
  typedef bool (*GetDataCallback)(int index, MediaItem& item, void* reference);

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
  DLNAMediaServer(HttpServer& server, IUDPService& udp) : DLNAMediaServer() {
    // use setters so derived classes or overrides get a consistent path
    setHttpServer(server);
    setUdpService(udp);
    dlna_device.setReference(this);
  }

  /// Destructor
  ~DLNAMediaServer() { end(); }

  /// Set the http server instance the MediaServer should use
  void setHttpServer(HttpServer& server) {
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
  bool loop() { return dlna_device.loop(); }

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
      result += out.print(StringRegistry::nullStr(self->getSourceProtocols()));
      result += out.print("\"/>\n");
      result += out.print("<SinkProtocolInfo val=\"");
      result += out.print(StringRegistry::nullStr(self->getSinkProtocols()));
      result += out.print("\"/>\n");
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

  /// Sets the callback that provides a MediaItem by index
  void setGetDataCallback(GetDataCallback cb) { get_data_cb = cb; }

  /// Sets a user reference pointer, available in callbacks
  void setReference(void* ref) { reference_ = ref; }

  /// Provides access to the http server
  HttpServer* getHttpServer() { return p_server; }

  /// Provides access to the system update ID
  int getSystemUpdateID() { return g_stream_updateID; }

  /// Call this method if content has changed : Increments and returns the
  /// SystemUpdateID
  int incrementSystemUpdateID() { return ++g_stream_updateID; }

  /// Provides access to the internal DLNA device instance
  DLNADevice& device() { return dlna_device; }

  /// Enable/disable subscription notifications
  void setSubscriptionsActive(bool flag) {
    dlna_device.setSubscriptionsActive(flag);
  }

  /// Query whether subscription notifications are active
  bool isSubscriptionsActive() { return dlna_device.isSubscriptionsActive(); }

  /// Define your own custom logic
  void setCustomActionRule(const char* suffix,
                           bool (*handler)(DLNAMediaServer*, ActionRequest&,
                                           HttpServer&)) {
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

 protected:
  // Action rule struct for ContentDirectory
  struct ActionRule {
    const char* suffix;
    bool (*handler)(DLNAMediaServer*, ActionRequest&, HttpServer&);
  };

  DLNADevice dlna_device;
  PrepareDataCallback prepare_data_cb = nullptr;
  GetDataCallback get_data_cb = nullptr;
  HttpServer* p_server = nullptr;
  IUDPService* p_udp_member = nullptr;
  void* reference_ = nullptr;
  GetDataCallback g_stream_get_data_cb = nullptr;
  int g_stream_numberReturned = 0;
  int g_stream_totalMatches = 0;
  int g_stream_updateID = 1;
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
      result += out.print("\"/>\n");
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
      result += out.print(StringRegistry::nullStr(self->getSourceProtocols()));
      result += out.print("\"/>\n");
      result += out.print("<SinkProtocolInfo val=\"");
      result += out.print(StringRegistry::nullStr(self->getSinkProtocols()));
      result += out.print("\"/>\n");
      result += out.print("<CurrentConnectionIDs val=\"");
      result += out.print(StringRegistry::nullStr(self->connectionID));
      result += out.print("\"/>\n");
      return result;
    };
    addChange("CMS", writer);
  }

  /// Setup the service endpoints
  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::setupServices");

    // register the individual services via helpers
    setupContentDirectoryService(server);
    setupConnectionManagerService(server);
  }

  /// Static descriptor callback for ContentDirectory SCPD
  static void contentDescCB(HttpServer* server, const char* requestPath,
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
  static void connDescCB(HttpServer* server, const char* requestPath,
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
  static void eventSubscriptionHandler(HttpServer* server,
                                       const char* requestPath,
                                       HttpRequestHandlerLine* hl) {
    DLNADevice::eventSubscriptionHandler(server, requestPath, hl);
    DLNAMediaServer* self = getMediaServer(server);
    assert(self != nullptr);
    if (self) {
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
  void setupContentDirectoryService(HttpServer* server) {
    DLNAServiceInfo cd;
    cd.setup("urn:schemas-upnp-org:service:ContentDirectory:1",
             "urn:upnp-org:serviceId:ContentDirectory", "/CD/service.xml",
             &DLNAMediaServer::contentDescCB, "/CD/control",
             &DLNAMediaServer::contentDirectoryControlCB, "/CD/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });

    // subscription namespace abbreviation used for event publishing
    cd.subscription_namespace_abbrev = "AVT";

    // register URLs on the provided server so SCPD, control and event
    // subscription endpoints are available immediately. The server stores
    // a reference to this MediaServer via setReference(), so we don't need
    // to provide per-handler context arrays.
    server->on(cd.scpd_url, T_GET, cd.scp_cb);
    server->on(cd.control_url, T_POST, cd.control_cb);
    server->on(cd.event_sub_url, T_SUBSCRIBE, eventSubscriptionHandler);
    server->on(cd.event_sub_url, T_UNSUBSCRIBE,
               tiny_dlna::DLNADevice::eventSubscriptionHandler);
    server->on(cd.event_sub_url, T_POST,
               tiny_dlna::DLNADevice::eventSubscriptionHandler);

    addService(cd);
  }

  /// Setup and register ConnectionManager service
  void setupConnectionManagerService(HttpServer* server) {
    DLNAServiceInfo cm;
    cm.setup("urn:schemas-upnp-org:service:ConnectionManager:1",
             "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
             &DLNAMediaServer::connDescCB, "/CM/control",
             &DLNAMediaServer::connmgrControlCB, "/CM/event",
             tiny_dlna::DLNADevice::eventSubscriptionHandler);

    // subscription namespace abbreviation used for event publishing
    cm.subscription_namespace_abbrev = "CMS";

    server->on(cm.scpd_url, T_GET, cm.scp_cb);
    server->on(cm.control_url, T_POST, cm.control_cb);
    server->on(cm.event_sub_url, T_SUBSCRIBE, eventSubscriptionHandler);
    server->on(cm.event_sub_url, T_UNSUBSCRIBE, cm.event_sub_cb);
    server->on(cm.event_sub_url, T_POST, cm.event_sub_cb);

    addService(cm);
  }

  /// Process action requests using rules-based dispatch
  bool processAction(ActionRequest& action, HttpServer& server) {
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
  bool processActionBrowse(ActionRequest& action, HttpServer& server) {
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

    // provide result
    return streamActionItems("BrowseResponse",
                             action.getArgumentValue("ObjectID"), qtype,
                             action.getArgumentValue("Filter"), startingIndex,
                             requestedCount, server);
  }

  /// Handle ContentDirectory:Search action
  bool processActionSearch(ActionRequest& action, HttpServer& server) {
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

    // provide result
    return streamActionItems("SearchResponse",
                             action.getArgumentValue("ContainerID"), qtype,
                             action.getArgumentValue("SearchCriteria"),
                             startingIndex, requestedCount, server);
  }

  /// Handle ContentDirectory:GetSearchCapabilities action
  bool processActionGetSearchCapabilities(ActionRequest& action,
                                          HttpServer& server) {
    ChunkPrint chunk{server.client()};
    server.replyChunked("text/xml");

    soapEnvelopeStart(chunk);
    actionResponseStart(chunk, "GetSearchCapabilitiesResponse",
                        "urn:schemas-upnp-org:service:ContentDirectory:1");

    chunk.print("<SearchCaps>");
    chunk.print(StringRegistry::nullStr(g_search_capabiities));
    chunk.println("</SearchCaps>");

    actionResponseEnd(chunk, "GetSearchCapabilitiesResponse");
    soapEnvelopeEnd(chunk);
    chunk.end();
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processActionGetSearchCapabilities (streamed)");
    return true;
  }

  /// Handle ContentDirectory:GetSortCapabilities action
  bool processActionGetSortCapabilities(ActionRequest& action,
                                        HttpServer& server) {
    ChunkPrint chunk{server.client()};
    server.replyChunked("text/xml");

    soapEnvelopeStart(chunk);
    actionResponseStart(chunk, "GetSortCapabilitiesResponse",
                        "urn:schemas-upnp-org:service:ContentDirectory:1");

    chunk.print("<SortCaps>");
    chunk.print(StringRegistry::nullStr(g_sort_capabilities));
    chunk.println("</SortCaps>");

    actionResponseEnd(chunk, "GetSortCapabilitiesResponse");
    soapEnvelopeEnd(chunk);
    chunk.end();
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processActionGetSortCapabilities (streamed)");
    return true;
  }

  /// Handle ContentDirectory:GetSystemUpdateID action
  bool processActionGetSystemUpdateID(ActionRequest& action,
                                      HttpServer& server) {
    ChunkPrint chunk{server.client()};
    server.replyChunked("text/xml");

    soapEnvelopeStart(chunk);
    actionResponseStart(chunk, "GetSystemUpdateIDResponse",
                        "urn:schemas-upnp-org:service:ContentDirectory:1");

    chunk.printf("<Id>%d</Id>\r\n", g_stream_updateID);

    actionResponseEnd(chunk, "GetSystemUpdateIDResponse");
    soapEnvelopeEnd(chunk);
    chunk.end();
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processActionGetSystemUpdateID (streamed): %d",
                   g_stream_updateID);
    return true;
  }

  /// Replies with Source and Sink protocol lists (CSV protocolInfo strings)
  bool processActionGetProtocolInfo(ActionRequest& action, HttpServer& server) {
    StrPrint resp;
    // Use shared helper and pass device-specific protocol lists
    DLNADevice::replyGetProtocolInfo(resp, sourceProto, sinkProto);
    server.reply("text/xml", resp.c_str());
    return true;
  }

  /// Handle ConnectionManager:GetCurrentConnectionIDs action
  bool processActionGetCurrentConnectionIDs(ActionRequest& action,
                                            HttpServer& server) {
    StrPrint resp;
    // return the currently configured connection ID(s)
    DLNADevice::replyGetCurrentConnectionIDs(resp, connectionID);
    server.reply("text/xml", resp.c_str());
    return true;
  }
  /// Handle ConnectionManager:GetCurrentConnectionInfo action
  bool processActionGetCurrentConnectionInfo(ActionRequest& action,
                                             HttpServer& server) {
    // Read requested ConnectionID (not used in this simple implementation)
    int connId = action.getArgumentIntValue("ConnectionID");

    // Stream the GetCurrentConnectionInfo response using chunked encoding
    // to avoid building the full response in memory and to remain
    // consistent with other handlers.
    StrPrint resp;
    DLNADevice::replyGetCurrentConnectionInfo(resp, sourceProto, connectionID,
                                              "Output");
    server.reply("text/xml", resp.c_str());
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

  void soapEnvelopeStart(ChunkPrint& chunk) {
    chunk.print("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
    chunk.print(
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "\r\n");
    chunk.print(
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n");
    chunk.print("<s:Body>\r\n");
  }

  void soapEnvelopeEnd(ChunkPrint& chunk) {
    chunk.print("</s:Body>\r\n");
    chunk.print("</s:Envelope>\r\n");
  }

  void actionResponseStart(ChunkPrint& chunk, const char* responseName,
                           const char* serviceNS) {
    // writes: <u:ResponseName xmlns:u="serviceNS">
    chunk.printf("<u:%s xmlns:u=\"%s\">\r\n", responseName, serviceNS);
  }

  void actionResponseEnd(ChunkPrint& chunk, const char* responseName) {
    chunk.printf("</u:%s>\r\n", responseName);
  }

  bool streamActionItems(const char* responseName, const char* objectID,
                         ContentQueryType queryType, const char* filter,
                         int startingIndex, int requestedCount,
                         HttpServer& server) {
    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = g_stream_updateID;

    // allow caller to prepare counts/updateID
    if (prepare_data_cb) {
      prepare_data_cb(objectID, queryType, filter, startingIndex,
                      requestedCount, nullptr, numberReturned, totalMatches,
                      updateID, reference_);
    }

    ChunkPrint chunk_writer{server.client()};
    server.replyChunked("text/xml");

    // SOAP envelope start and response wrapper
    soapEnvelopeStart(chunk_writer);
    // responseName is assumed to be e.g. "BrowseResponse" or "SearchResponse"
    actionResponseStart(chunk_writer, responseName,
                        "urn:schemas-upnp-org:service:ContentDirectory:1");

    // Result -> DIDL-Lite
    chunk_writer.println("<Result>");
    // DIDL root with namespaces
    streamDIDL(chunk_writer, numberReturned, startingIndex);
    chunk_writer.print("</Result>\r\n");

    // numeric fields
    chunk_writer.printf("<NumberReturned>%d</NumberReturned>\r\n",
                        numberReturned);
    chunk_writer.printf("<TotalMatches>%d</TotalMatches>\r\n", totalMatches);
    chunk_writer.printf("<UpdateID>%d</UpdateID>\r\n", updateID);

    actionResponseEnd(chunk_writer, responseName);
    soapEnvelopeEnd(chunk_writer);
    chunk_writer.end();
    return true;
  }

  /// Stream DIDL-Lite payload for a Browse/Search result
  void streamDIDL(ChunkPrint& chunk_writer, int numberReturned,
                  int startingIndex) {
    // Stream DIDL-Lite Result payload using helper
    chunk_writer.print(
        "&lt;DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
        "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\"&gt;\r\n");

    if (get_data_cb) {
      Str url{160};
      for (int i = 0; i < numberReturned; ++i) {
        MediaItem item;
        int idx = startingIndex + i;
        if (!get_data_cb(idx, item, reference_)) break;

        const char* mediaItemClassStr = toStr(item.itemClass);
        const char* nodeName =
            (item.itemClass == MediaItemClass::Folder) ? "container" : "item";

        chunk_writer.printf(
            "&lt;%s id=\"%s\" parentID=\"%s\" restricted=\"%d\"&gt;", nodeName,
            item.id ? item.id : "", item.parentID ? item.parentID : "0",
            item.restricted ? 1 : 0);

        // title
        chunk_writer.print("&lt;dc:title&gt;");
        chunk_writer.print(StringRegistry::nullStr(item.title));
        chunk_writer.print("&lt;/dc:title&gt;\r\n");
        if (mediaItemClassStr != nullptr) {
          chunk_writer.printf("&lt;upnp:class&gt;%s&lt;/upnp:class&gt;\r\n",
                              mediaItemClassStr);
        }

        // res with optional protocolInfo attribute
        url.set(item.resourceURL);
        if (!url.isEmpty()) {
          url.replaceAll("&", "&amp;");
          if (!StrView(item.mimeType).isEmpty()) {
            chunk_writer.printf("&lt;res protocolInfo=\"http-get:*:%s:*\"&gt;",
                                item.mimeType);
            chunk_writer.print(StringRegistry::nullStr(url.c_str()));
            chunk_writer.print("&lt;/res&gt;\r\n");
          } else {
            chunk_writer.print("&lt;res&gt;");
            chunk_writer.print(StringRegistry::nullStr(url.c_str()));
            chunk_writer.print("&lt;/res&gt;\r\n");
          }
        }

        chunk_writer.printf("&lt;/%s&gt;\r\n", nodeName);
      }
    }

    chunk_writer.print("&lt;/DIDL-Lite&gt;\r\n");
  }

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
  static void contentDirectoryControlCB(HttpServer* server,
                                        const char* requestPath,
                                        HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DLNADevice::parseActionRequest(server, requestPath, hl, action);

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
   * @brief Helper to retrieve DLNAMediaServer instance from HttpServer reference chain
   * 
   * Navigates through the server's reference (DLNADevice) to get the MediaServer instance.
   * 
   * @param server Pointer to the HttpServer instance
   * @return Pointer to the DLNAMediaServer instance, or nullptr if not found
   */
  static DLNAMediaServer* getMediaServer(HttpServer* server) {
    if (!server) return nullptr;
    DLNADevice* dev = (DLNADevice*)server->getReference();
    if (!dev) return nullptr;
    return (DLNAMediaServer*)dev->getReference();
  }

  /// simple connection manager control that replies OK
  static void connmgrControlCB(HttpServer* server, const char* requestPath,
                               HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DLNADevice::parseActionRequest(server, requestPath, hl, action);

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
                                  HttpServer& server) {
                       return self->processActionBrowse(action, server);
                     }});
    rules.push_back({"Search", [](DLNAMediaServer* self, ActionRequest& action,
                                  HttpServer& server) {
                       return self->processActionSearch(action, server);
                     }});
    rules.push_back(
        {"GetSearchCapabilities",
         [](DLNAMediaServer* self, ActionRequest& action, HttpServer& server) {
           return self->processActionGetSearchCapabilities(action, server);
         }});
    rules.push_back(
        {"GetSortCapabilities",
         [](DLNAMediaServer* self, ActionRequest& action, HttpServer& server) {
           return self->processActionGetSortCapabilities(action, server);
         }});
    rules.push_back(
        {"GetSystemUpdateID",
         [](DLNAMediaServer* self, ActionRequest& action, HttpServer& server) {
           return self->processActionGetSystemUpdateID(action, server);
         }});
    // ConnectionManager rules
    rules.push_back(
        {"GetProtocolInfo",
         [](DLNAMediaServer* self, ActionRequest& action, HttpServer& server) {
           return self->processActionGetProtocolInfo(action, server);
         }});
    rules.push_back(
        {"GetCurrentConnectionIDs",
         [](DLNAMediaServer* self, ActionRequest& action, HttpServer& server) {
           return self->processActionGetCurrentConnectionIDs(action, server);
         }});
    rules.push_back(
        {"GetCurrentConnectionInfo",
         [](DLNAMediaServer* self, ActionRequest& action, HttpServer& server) {
           return self->processActionGetCurrentConnectionInfo(action, server);
         }});
  }
};

}  // namespace tiny_dlna
