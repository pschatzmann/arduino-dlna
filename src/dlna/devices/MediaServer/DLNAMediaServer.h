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

/**
 * @brief Digital Media Server implementation
 *
 * This class implements a lightweight DLNA MediaServer device with a
 * ContentDirectory service (Browse and Search) and a ConnectionManager service.
 *
 * The API is designed for embedding custom content lists and streaming logic.
 * The API uses two callbacks to retrieve the content data:
 *   - PrepareDataCallback: Called to determine the number of items, total
 * matches, and update ID for a Browse or Search request.
 *   - GetDataCallback: Called for each item to retrieve its metadata
 * (MediaItem) by index.
 *
 * A user reference pointer can be set with setReference(void*), and is passed
 * to both callbacks for custom context or data.
 *
 * Example usage:
 *   - Implement PrepareDataCallback and GetDataCallback.
 *   - Register them with setPrepareDataCallback() and setGetDataCallback().
 *   - Optionally set a reference pointer with setReference().
 *
 * The implementation is intentionally compact and suitable as a starting
 * point for embedded or test DLNA servers.
 */
class DLNAMediaServer : public DLNADeviceInfo {
 public:
  /**
   * @brief Callback signature for preparing data for Browse or Search requests.
   * @param objectID Object ID to browse/search
   * @param queryType Content query type (Browse/Metadata/Search)
   * @param filter Filter string
   * @param startingIndex Starting index for paging
   * @param requestedCount Number of items requested
   * @param sortCriteria Sort criteria string
   * @param numberReturned [out] Number of items returned
   * @param totalMatches [out] Total number of matches
   * @param updateID [out] System update ID
   * @param reference User reference pointer
   */
  typedef void (*PrepareDataCallback)(
      const char* objectID, ContentQueryType queryType, const char* filter,
      int startingIndex, int requestedCount, const char* sortCriteria,
      int& numberReturned, int& totalMatches, int& updateID, void* reference);

  /**
   * @brief Callback signature for retrieving a MediaItem by index.
   * @param index Item index
   * @param item [out] MediaItem metadata
   * @param reference User reference pointer
   * @return true if item is valid, false otherwise
   */
  typedef bool (*GetDataCallback)(int index, MediaItem& item, void* reference);

  /**
   * @brief Default constructor for MediaServer.
   * Initializes device information and default properties.
   */
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
  /**
   * @brief Recommended constructor
   * Construct a MediaServer with associated HTTP server and UDP service
   * This constructor stores the provided server/udp references so begin() can
   * be called without parameters.
   */
  DLNAMediaServer(HttpServer& server, IUDPService& udp) : DLNAMediaServer() {
    // use setters so derived classes or overrides get a consistent path
    setHttpServer(server);
    setUdpService(udp);
  }

  /// Destructor
  ~DLNAMediaServer() { end(); }

  /// Set the http server instance the MediaServer should use
  void setHttpServer(HttpServer& server) {
    // ensure instance pointer is available for callbacks
    self = this;
    p_server = &server;
    // prepare handler context and register service endpoints on the provided
    // server
    ref_ctx[0] = this;
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
    StrPrint cmd;
    cmd.print("<SourceProtocolInfo>\n");
    cmd.print(StringRegistry::nullStr(sourceProto));
    cmd.print("</SourceProtocolInfo>\n");
    cmd.print("<SinkProtocolInfo>\n");
    cmd.print(StringRegistry::nullStr(sinkProto));
    cmd.print("</SinkProtocolInfo>\n");
    addChange("CMS", cmd.c_str());
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

 protected:
  // Action rule struct for ContentDirectory
  struct ActionRule {
    const char* suffix;
    bool (*handler)(DLNAMediaServer*, ActionRequest&, HttpServer&);
  };

  static inline DLNAMediaServer* self = nullptr;
  // internal DLNA device instance owned by this MediaServer
  DLNADevice dlna_device;
  // stored callbacks
  PrepareDataCallback prepare_data_cb = nullptr;
  GetDataCallback get_data_cb = nullptr;

  HttpServer* p_server = nullptr;
  IUDPService* p_udp_member = nullptr;
  void* ref_ctx[1] = {nullptr};
  void* reference_ = nullptr;
  GetDataCallback g_stream_get_data_cb = nullptr;
  int g_stream_numberReturned = 0;
  int g_stream_totalMatches = 0;
  int g_stream_updateID = 1;
  int g_instance_id = 1;
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

  /// serviceAbbrev: e.g. "AVT" or  or subscription namespace abbrev defined for
  /// the service
  void addChange(const char* serviceAbbrev, const char* changeTag) {
    // Delegate to the internal DLNADevice instance which manages
    // subscriptions and services.
    dlna_device.addChange(serviceAbbrev, changeTag);
  }

  /// Publish a ContentDirectory event (SystemUpdateID)
  void publishAVT() {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::publishAVT");
    StrPrint cmd;
    cmd.printf("<SystemUpdateID val=\"%d\"/>\n", g_stream_updateID);
    addChange("AVT", cmd.c_str());
  }

  /// Publish a ConnectionManager event (CurrentConnectionIDs)
  void publishCMS() {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::publishCMS");
    StrPrint cmd;
    cmd.print("<SourceProtocolInfo>\n");
    cmd.print(StringRegistry::nullStr(sourceProto));
    cmd.print("</SourceProtocolInfo>\n");
    cmd.print("<SinkProtocolInfo>\n");
    cmd.print(StringRegistry::nullStr(sinkProto));
    cmd.print("</SinkProtocolInfo>\n");
    cmd.printf("<CurrentConnectionIDs>%s</CurrentConnectionIDs>\n",
               connectionID);
    addChange("CMS", cmd.c_str());
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
    server->reply("text/xml", [](Print& out) {
      tiny_dlna::DLNAMediaServerContentDirectoryDescr descr;
      descr.printDescr(out);
    });
  }

  /// Static descriptor callback for ConnectionManager SCPD
  static void connDescCB(HttpServer* server, const char* requestPath,
                         HttpRequestHandlerLine* hl) {
    server->reply("text/xml", [](Print& out) {
      tiny_dlna::DLNAMediaServerConnectionMgrDescr descr;
      descr.printDescr(out);
    });
  }

  /// After the subscription we publish all relevant properties
  static void eventSubscriptionHandler(HttpServer* server,
                                       const char* requestPath,
                                       HttpRequestHandlerLine* hl) {
    DLNADevice::eventSubscriptionHandler(server, requestPath, hl);
    DLNAMediaServer* device = (DLNAMediaServer*)(hl->context[0]);
    if (device) {
      StrView request_path_str(requestPath);
      if (request_path_str.contains("/CD/"))
        device->publishAVT();
      else if (request_path_str.contains("/CM/"))
        device->publishCMS();
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
    // subscription endpoints are available immediately
    server->on(cd.scpd_url, T_GET, cd.scp_cb, ref_ctx, 1);
    server->on(cd.control_url, T_POST, cd.control_cb, ref_ctx, 1);
    // Register event subscription handlers for SUBSCRIBE, UNSUBSCRIBE and
    // POST so the server accepts subscription requests and related POSTS on
    // the same event URL.
    server->on(cd.event_sub_url, T_SUBSCRIBE, eventSubscriptionHandler, ref_ctx,
               1);
    server->on(cd.event_sub_url, T_UNSUBSCRIBE,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);
    server->on(cd.event_sub_url, T_POST,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);

    addService(cd);
  }

  /// Setup and register ConnectionManager service
  void setupConnectionManagerService(HttpServer* server) {
    DLNAServiceInfo cm;
    cm.setup("urn:schemas-upnp-org:service:ConnectionManager:1",
             "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
             &DLNAMediaServer::connDescCB, "/CM/control",
             &DLNAMediaServer::connmgrControlCB, "/CM/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });

    // subscription namespace abbreviation used for event publishing
    cm.subscription_namespace_abbrev = "CMS";

    server->on(cm.scpd_url, T_GET, cm.scp_cb, ref_ctx, 1);
    server->on(cm.control_url, T_POST, cm.control_cb, ref_ctx, 1);
    // Register event subscription handlers for SUBSCRIBE, UNSUBSCRIBE and
    // POST so the server accepts subscription requests and related POSTS on
    // the same event URL.
    server->on(cm.event_sub_url, T_SUBSCRIBE, eventSubscriptionHandler, ref_ctx,
               1);
    server->on(cm.event_sub_url, T_UNSUBSCRIBE,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);
    server->on(cm.event_sub_url, T_POST,
               tiny_dlna::DLNADevice::eventSubscriptionHandler, ref_ctx, 1);

    addService(cm);
  }

  /// Process action requests using rules-based dispatch
  bool processAction(ActionRequest& action, HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "DLNAMediaServer::processAction: %s",
                   StringRegistry::nullStr(action.action));
    StrView action_str(action.action);
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
                   action.action);
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
    // Stream the GetProtocolInfo response using chunked encoding to avoid
    // building large in-memory strings and to keep behavior consistent with
    // other actions (e.g. Browse/Search).
    ChunkPrint chunk{server.client()};
    server.replyChunked("text/xml");

    soapEnvelopeStart(chunk);
    actionResponseStart(chunk, "GetProtocolInfoResponse",
                        "urn:schemas-upnp-org:service:ConnectionManager:1");

    chunk.print("<Source>");
    chunk.print(StringRegistry::nullStr(sourceProto));
    chunk.println("</Source>");

    chunk.print("<Sink>");
    chunk.print(StringRegistry::nullStr(sinkProto));
    chunk.println("</Sink>");

    actionResponseEnd(chunk, "GetProtocolInfoResponse");
    soapEnvelopeEnd(chunk);
    chunk.end();
    return true;
  }

  /// Handle ConnectionManager:GetCurrentConnectionIDs action
  bool processActionGetCurrentConnectionIDs(ActionRequest& action,
                                            HttpServer& server) {
    // Stream the empty ConnectionIDs response using chunked encoding to avoid
    // allocating the full response in memory and keep consistent streaming
    // behavior with other services.
    ChunkPrint chunk{server.client()};
    server.replyChunked("text/xml");

    soapEnvelopeStart(chunk);
    actionResponseStart(chunk, "GetCurrentConnectionIDsResponse",
                        "urn:schemas-upnp-org:service:ConnectionManager:1");

    // list with one default value now
    chunk.println("<ConnectionIDs>01</ConnectionIDs>");

    actionResponseEnd(chunk, "GetCurrentConnectionIDsResponse");
    soapEnvelopeEnd(chunk);
    chunk.end();
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
    ChunkPrint chunk{server.client()};
    server.replyChunked("text/xml");

    soapEnvelopeStart(chunk);
    actionResponseStart(chunk, "GetCurrentConnectionInfoResponse",
                        "urn:schemas-upnp-org:service:ConnectionManager:1");

    // RcsID and AVTransportID
    chunk.println("<RcsID>0</RcsID>");
    chunk.println("<AVTransportID>0</AVTransportID>");

    // ProtocolInfo (empty by default)
    chunk.print("<ProtocolInfo>");
    chunk.print(StringRegistry::nullStr(sourceProto));
    chunk.println("</ProtocolInfo>");

    // PeerConnectionManager and PeerConnectionID
    chunk.print("<PeerConnectionManager>");
    chunk.println("</PeerConnectionManager>");
    chunk.println("<PeerConnectionID>0</PeerConnectionID>");

    // Direction and Status
    chunk.println("<Direction>Output</Direction><Status>OK</Status>");

    actionResponseEnd(chunk, "GetCurrentConnectionInfoResponse");
    soapEnvelopeEnd(chunk);
    chunk.end();
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
    DLNAMediaServer* ms = nullptr;
    if (hl && hl->context[0]) ms = (DLNAMediaServer*)hl->context[0];

    // process the requested action using instance method if available
    if (ms) {
      if (ms->processAction(action, *server)) return;
    }

    server->replyNotFound();
  }

  /// simple connection manager control that replies OK
  static void connmgrControlCB(HttpServer* server, const char* requestPath,
                               HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DLNADevice::parseActionRequest(server, requestPath, hl, action);

    DLNAMediaServer* ms = nullptr;
    if (hl && hl->context[0]) ms = (DLNAMediaServer*)hl->context[0];
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
