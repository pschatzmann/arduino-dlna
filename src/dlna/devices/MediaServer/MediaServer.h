// Minimal header-only Digital Media Server (MediaServer) device
#pragma once

#include "MediaItem.h"
#include "basic/EscapingPrint.h"
#include "basic/Str.h"
#include "basic/StrPrint.h"
#include "dlna/DLNADevice.h"
#include "dlna/devices/MediaServer/MediaItem.h"
#include "dlna/service/Action.h"
#include "dlna/xml/XMLParserPrint.h"
#include "dlna/xml/XMLPrinter.h"
#include "http/HttpServer.h"
#include "ms_connmgr.h"
#include "ms_content_dir.h"

namespace tiny_dlna {

/**
 * @brief Minimal Digital Media Server implementation
 *
 * This class implements a lightweight DLNA MediaServer device with a
 * ContentDirectory service (Browse) and a ConnectionManager service.
 *
 * The API is designed for embedding custom content lists and streaming logic.
 * Instead of a single browse callback, the API now uses two callbacks:
 *   - PrepareDataCallback: Called to determine the number of items, total
 * matches, and update ID for a Browse request.
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
class MediaServer : public DLNADeviceInfo {
 public:
  // PrepareData callback signature:
  // objectID, browseFlag, filter, startingIndex, requestedCount, sortCriteria,
  // numberReturned (out), totalMatches (out), updateID (out), reference
  typedef void (*PrepareDataCallback)(
      const char* objectID, const char* browseFlag, const char* filter,
      int startingIndex, int requestedCount, const char* sortCriteria,
      int& numberReturned, int& totalMatches, int& updateID, void* reference);

  // GetData callback signature:
  // index, MediaItem (out), reference
  typedef bool (*GetDataCallback)(int index, MediaItem& item, void* reference);

  MediaServer() {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::MediaServer");
    setSerialNumber(usn);
    setDeviceType(st);
    setFriendlyName("MediaServer");
    setManufacturer("TinyDLNA");
    setModelName("TinyDLNA MediaServer");
    setModelNumber("1.0");
    setBaseURL("http://localhost:44757");
  }
  ~MediaServer() { end(); }

  void end() { /* nothing special */ }

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
  void setSourceProtocols(const char* protos) { sourceProto = protos; }

  /// Get the current source `ProtocolInfo` string
  const char* getSourceProtocols() { return sourceProto; }

  /// Defines the sink protocol info: use csv: default ""
  void setSinkProtocols(const char* protos) { sinkProto = protos; }

  /// Get the current sink `ProtocolInfo` string
  const char* getSinkProtocols() { return sinkProto; }

  /// initialization called by begin()
  void setupServices(HttpServer& server, IUDPService& udp) {
    p_media_server = this;
    setupServicesImpl(&server);
    p_server = &server;
  }

  /// Sets the PrepareData callback
  void setPrepareDataCallback(PrepareDataCallback cb) { prepare_data_cb = cb; }

  /// Sets the GetData callback
  void setGetDataCallback(GetDataCallback cb) { get_data_cb = cb; }

  /// Sets a user reference pointer, available in callbacks
  void setReference(void* ref) { reference_ = ref; }

  /// Provides access to the http server
  HttpServer* getHttpServer() { return p_server; }

  /// Provides access to the system update ID
  int getSystemUpdateID() { return g_stream_updateID; }

  /// Increments and returns the SystemUpdateID
  int incrementSystemUpdateID() { return ++g_stream_updateID; }

 protected:
  static inline MediaServer* p_media_server = nullptr;
  const char* st = "urn:schemas-upnp-org:device:MediaServer:1";
  const char* usn = "uuid:media-server-0000-0000-0000-000000000001";
  // stored callbacks
  PrepareDataCallback prepare_data_cb = nullptr;
  GetDataCallback get_data_cb = nullptr;

  HttpServer* p_server = nullptr;
  void* reference_ = nullptr;
  GetDataCallback g_stream_get_data_cb = nullptr;
  int g_stream_numberReturned = 0;
  int g_stream_totalMatches = 0;
  int g_stream_updateID = 0;
  void* g_stream_reference = nullptr;
  const char* g_search_capabiities =
      "dc:title,dc:creator,upnp:class,upnp:genre,"
      "upnp:album,upnp:artist,upnp:albumArtURI";
  const char* g_sort_capabilities =
      "dc:title,dc:date,upnp:class,upnp:album,upnp:episodeNumber,upnp:"
      "originalTrackNumber";
  // Default protocol info values (empty lists)
  const char* sourceProto =
      "http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN,http-get:*:image/"
      "jpeg:DLNA.ORG_PN=JPEG_SM,http-get:*:image/"
      "jpeg:DLNA.ORG_PN=JPEG_MED,http-get:*:image/"
      "jpeg:DLNA.ORG_PN=JPEG_LRG,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_HD_50_AC3_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_HD_60_AC3_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=MPEG_PS_NTSC,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=MPEG_PS_PAL,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=MPEG_TS_HD_NA_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=MPEG_TS_SD_NA_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=MPEG_TS_SD_EU_ISO,http-get:*:video/"
      "mpeg:DLNA.ORG_PN=MPEG1,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AAC_MULT5,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AC3,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF15_AAC_520,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF30_AAC_940,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_BL_L31_HD_AAC,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_BL_L32_HD_AAC,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_HP_HD_AAC,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_1080i_AAC,http-get:*:video/"
      "mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_720p_AAC,http-get:*:video/"
      "mp4:DLNA.ORG_PN=MPEG4_P2_MP4_ASP_AAC,http-get:*:video/"
      "mp4:DLNA.ORG_PN=MPEG4_P2_MP4_SP_VGA_AAC,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU_T,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA,http-get:*:video/"
      "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA_T,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVSPLL_BASE,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVSPML_BASE,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVSPML_MP3,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVMED_BASE,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVMED_FULL,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVMED_PRO,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVHIGH_FULL,http-get:*:video/"
      "x-ms-wmv:DLNA.ORG_PN=WMVHIGH_PRO,http-get:*:video/"
      "3gpp:DLNA.ORG_PN=MPEG4_P2_3GPP_SP_L0B_AAC,http-get:*:video/"
      "3gpp:DLNA.ORG_PN=MPEG4_P2_3GPP_SP_L0B_AMR,http-get:*:audio/"
      "mpeg:DLNA.ORG_PN=MP3,http-get:*:audio/"
      "x-ms-wma:DLNA.ORG_PN=WMABASE,http-get:*:audio/"
      "x-ms-wma:DLNA.ORG_PN=WMAFULL,http-get:*:audio/"
      "x-ms-wma:DLNA.ORG_PN=WMAPRO,http-get:*:audio/"
      "x-ms-wma:DLNA.ORG_PN=WMALSL,http-get:*:audio/"
      "x-ms-wma:DLNA.ORG_PN=WMALSL_MULT5,http-get:*:audio/"
      "mp4:DLNA.ORG_PN=AAC_ISO_320,http-get:*:audio/"
      "3gpp:DLNA.ORG_PN=AAC_ISO_320,http-get:*:audio/"
      "mp4:DLNA.ORG_PN=AAC_ISO,http-get:*:audio/"
      "mp4:DLNA.ORG_PN=AAC_MULT5_ISO,http-get:*:audio/"
      "L16;rate=44100;channels=2:DLNA.ORG_PN=LPCM,http-get:*:image/"
      "jpeg:*,http-get:*:video/avi:*,http-get:*:video/divx:*,http-get:*:video/"
      "x-matroska:*,http-get:*:video/mpeg:*,http-get:*:video/"
      "mp4:*,http-get:*:video/x-ms-wmv:*,http-get:*:video/"
      "x-msvideo:*,http-get:*:video/x-flv:*,http-get:*:video/"
      "x-tivo-mpeg:*,http-get:*:video/quicktime:*,http-get:*:audio/"
      "mp4:*,http-get:*:audio/x-wav:*,http-get:*:audio/"
      "x-flac:*,http-get:*:audio/x-dsd:*,http-get:*:application/"
      "ogg:*http-get:*:application/vnd.rn-realmedia:*http-get:*:application/"
      "vnd.rn-realmedia-vbr:*http-get:*:video/webm:*";
  const char* sinkProto = "";
  const char* connectionID = "0";

  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::setupServices");

    auto contentDescCB = [](HttpServer* server, const char* requestPath,
                            HttpRequestHandlerLine* hl) {
      server->reply("text/xml",
                    [](Print& out) { ms_content_dir_xml_printer(out); });
    };
    auto connDescCB = [](HttpServer* server, const char* requestPath,
                         HttpRequestHandlerLine* hl) {
      server->reply("text/xml",
                    [](Print& out) { ms_connmgr_xml_printer(out); });
    };

    DLNAServiceInfo cd;
    cd.setup("urn:schemas-upnp-org:service:ContentDirectory:1",
             "urn:upnp-org:serviceId:ContentDirectory", "/CD/service.xml",
             contentDescCB, "/CD/control", contentDirectoryControlCB,
             "/CD/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });

    DLNAServiceInfo cm;
    cm.setup("urn:schemas-upnp-org:service:ConnectionManager:1",
             "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
             connDescCB, "/CM/control", connmgrControlCB, "/CM/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });

    addService(cd);
    addService(cm);
  }

  /// Process action requests
  bool processAction(ActionRequest& action, HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "processAction");
    StrView action_str(action.action);
    if (action_str.isEmpty()) {
      DlnaLogger.log(DlnaLogLevel::Error, "Empty action received");
      server.replyNotFound();
      return false;
    }
    if (action_str.endsWith("Browse")) {
      return processActionBrowse(action, server);
    } else if (action_str.endsWith("Search")) {
      return processActionSearch(action, server);
    } else if (action_str.endsWith("GetSearchCapabilities")) {
      return processActionGetSearchCapabilities(action, server);
    } else if (action_str.endsWith("GetSortCapabilities")) {
      return processActionGetSortCapabilities(action, server);
    } else if (action_str.endsWith("GetSystemUpdateID")) {
      return processActionGetSystemUpdateID(action, server);
    } else {
      DlnaLogger.log(DlnaLogLevel::Error, "Unsupported action: %s",
                     action.action);
      return false;
      server.replyNotFound();
    }
  }

  // --- Stub handlers for ContentDirectory actions ---
  // These are minimal implementations that currently reply with an empty
  // ActionResponse and return true. They exist as placeholders until
  // full implementations are added.
  bool processActionBrowse(ActionRequest& action, HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "processActionBrowse");
    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = g_stream_updateID;

    // extract paging args
    int startingIndex = action.getArgumentIntValue("StartingIndex");
    int requestedCount = action.getArgumentIntValue("RequestedCount");

    // query for data
    if (prepare_data_cb) {
      prepare_data_cb(action.getArgumentValue("ObjectID"),
                      action.getArgumentValue("BrowseFlag"),
                      action.getArgumentValue("Filter"), startingIndex,
                      requestedCount, action.getArgumentValue("SortCriteria"),
                      numberReturned, totalMatches, updateID, reference_);
    }

    // provide result
    return streamActionItems("BrowseResponse",
                             action.getArgumentValue("ObjectID"),
                             action.getArgumentValue("BrowseFlag"),
                             action.getArgumentValue("Filter"), startingIndex,
                             requestedCount, server);
  }

  bool processActionSearch(ActionRequest& action, HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "processActionSearch");

    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = g_stream_updateID;

    // extract paging args
    int startingIndex = action.getArgumentIntValue("StartingIndex");
    int requestedCount = action.getArgumentIntValue("RequestedCount");

    // query for data
    if (prepare_data_cb) {
      prepare_data_cb(action.getArgumentValue("ObjectID"),
                      action.getArgumentValue("BrowseFlag"),
                      action.getArgumentValue("Filter"), startingIndex,
                      requestedCount, action.getArgumentValue("SortCriteria"),
                      numberReturned, totalMatches, updateID, reference_);
    }

    // provide result
    return streamActionItems("SearchResponse",
                             action.getArgumentValue("ContainerID"), "Search",
                             action.getArgumentValue("SearchCriteria"),
                             startingIndex, requestedCount, server);
  }

  bool processActionGetSearchCapabilities(ActionRequest& action,
                                          HttpServer& server) {
    Str reply_str{replyTemplate()};
    reply_str.replaceAll("%1", "GetSearchCapabilitiesResponse");
    reply_str.replaceAll("%2", "SearchCaps");
    reply_str.replaceAll("%3", g_search_capabiities);
    server.reply("text/xml", reply_str.c_str());
    DlnaLogger.log(DlnaLogLevel::Info, "processActionGetSearchCapabilities: %s",
                   reply_str.c_str());
    return true;
  }

  bool processActionGetSortCapabilities(ActionRequest& action,
                                        HttpServer& server) {
    Str reply_str{replyTemplate()};
    reply_str.replaceAll("%2", "SortCaps");
    reply_str.replaceAll("%1", "GetSortCapabilitiesResponse");
    reply_str.replaceAll("%3", g_sort_capabilities);
    server.reply("text/xml", reply_str.c_str());

    DlnaLogger.log(DlnaLogLevel::Info, "processActionGetSortCapabilities: %s",
                   reply_str.c_str());
    return true;
  }

  bool processActionGetSystemUpdateID(ActionRequest& action,
                                      HttpServer& server) {
    Str reply_str{replyTemplate()};
    char update_id_str[80];
    sprintf(update_id_str, "%d", g_stream_updateID);
    reply_str.replaceAll("%2", "Id");
    reply_str.replaceAll("%1", "GetSystemUpdateIDResponse");
    reply_str.replaceAll("%3", update_id_str);
    server.reply("text/xml", reply_str.c_str());
    DlnaLogger.log(DlnaLogLevel::Info, "processActionGetSystemUpdateID: %s",
                   reply_str.c_str());
    return true;
  }

  // ConnectionManager action handlers (instance methods)
  /// Handle ConnectionManager:GetProtocolInfo action
  /// Replies with Source and Sink protocol lists (CSV protocolInfo strings)
  bool processActionGetProtocolInfo(ActionRequest& action, HttpServer& server) {
    Str reply = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    reply +=
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" ";
    reply += "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
    reply += "<s:Body>";
    reply +=
        "<u:GetProtocolInfoResponse "
        "xmlns:u=\"urn:schemas-upnp-org:service:ConnectionManager:1\">";
    reply += "<Source>";
    reply += sourceProto;
    reply += "</Source><Sink>";
    reply += sinkProto;
    reply += "</Sink></u:GetProtocolInfoResponse></s:Body></s:Envelope>";
    server.reply("text/xml", reply.c_str());
    return true;
  }

  /// Handle ConnectionManager:GetCurrentConnectionIDs action
  /// Replies with a comma-separated list of active ConnectionIDs
  bool processActionGetCurrentConnectionIDs(ActionRequest& action,
                                            HttpServer& server) {
    // return empty list
    Str reply = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    reply +=
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" ";
    reply += "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
    reply += "<s:Body>";
    reply +=
        "<u:GetCurrentConnectionIDsResponse "
        "xmlns:u=\"urn:schemas-upnp-org:service:ConnectionManager:1\">";
    reply +=
        "<ConnectionIDs></ConnectionIDs></u:GetCurrentConnectionIDsResponse></"
        "s:Body></s:Envelope>";
    server.reply("text/xml", reply.c_str());
    return true;
  }

  /// Handle ConnectionManager:GetCurrentConnectionInfo action
  /// Replies with information about the requested ConnectionID (RcsID,
  /// AVTransportID, ProtocolInfo, PeerConnectionManager/ID, Direction, Status)
  bool processActionGetCurrentConnectionInfo(ActionRequest& action,
                                             HttpServer& server) {
    // Read requested ConnectionID (not used in this simple implementation)
    int connId = action.getArgumentIntValue("ConnectionID");
    // Provide default info: no peer, protocol empty, status OK
    Str reply = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    reply +=
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" ";
    reply += "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
    reply += "<s:Body>";
    reply +=
        "<u:GetCurrentConnectionInfoResponse "
        "xmlns:u=\"urn:schemas-upnp-org:service:ConnectionManager:1\">";
    // RcsID, AVTransportID
    reply += "<RcsID>0</RcsID><AVTransportID>0</AVTransportID>";
    // ProtocolInfo
    reply += "<ProtocolInfo></ProtocolInfo>";
    // PeerConnectionManager and PeerConnectionID
    reply +=
        "<PeerConnectionManager></PeerConnectionManager><PeerConnectionID>0</"
        "PeerConnectionID>";
    // Direction and Status
    reply += "<Direction>Output</Direction><Status>OK</Status>";
    reply += "</u:GetCurrentConnectionInfoResponse></s:Body></s:Envelope>";
    server.reply("text/xml", reply.c_str());
    return true;
  }

  /// Common helper to stream a ContentDirectory response (Browse or Search)
  bool streamActionItems(const char* responseName, const char* objectID,
                         const char* browseFlag, const char* filter,
                         int startingIndex, int requestedCount,
                         HttpServer& server) {
    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = g_stream_updateID;

    // allow caller to prepare counts/updateID
    if (prepare_data_cb) {
      prepare_data_cb(objectID, browseFlag, filter, startingIndex,
                      requestedCount, nullptr, numberReturned, totalMatches,
                      updateID, reference_);
    }

    ChunkPrint chunk_writer{server.client()};
    server.replyChunked("text/xml");

    // SOAP envelope start
    chunk_writer.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    chunk_writer.println(
        "<s:Envelope "
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n");
    chunk_writer.println("<s:Body>");
    // responseName is assumed to be e.g. "BrowseResponse" or "SearchResponse"
    chunk_writer.printf(
        "<u:%s "
        "xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\r\n",
        responseName);

    // Result -> DIDL-Lite
    chunk_writer.println("<Result>");
    // DIDL root with namespaces
    chunk_writer.print(
        "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" ");
    chunk_writer.println(
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
        "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">\r\n");

    if (get_data_cb) {
      for (int i = 0; i < requestedCount; ++i) {
        MediaItem item;
        int idx = startingIndex + i;
        if (!get_data_cb(idx, item, reference_)) break;
        numberReturned++;

        char tmp[400];
        snprintf(tmp, sizeof(tmp),
                 "<item id=\"%s\" parentID=\"%s\" restricted=\"%d\">",
                 item.id ? item.id : "", item.parentID ? item.parentID : "0",
                 item.restricted ? 1 : 0);
        chunk_writer.println(tmp);

        // title
        chunk_writer.print("<dc:title>");
        chunk_writer.printEscaped(item.title ? item.title : "");
        chunk_writer.println("</dc:title>");

        // res with optional protocolInfo attribute
        if (item.mimeType) {
          snprintf(tmp, sizeof(tmp), "<res protocolInfo=\"%s\">",
                   item.mimeType);
          chunk_writer.println(tmp);
          chunk_writer.printEscaped(item.res ? item.res : "");
          chunk_writer.println("</res>");
        } else {
          chunk_writer.print("<res>");
          chunk_writer.printEscaped(item.res ? item.res : "");
          chunk_writer.println("</res>");
        }

        chunk_writer.println("</item>");
      }
    }

    chunk_writer.println("</DIDL-Lite>");
    chunk_writer.println("</Result>");

    // numeric fields
    chunk_writer.printf("<NumberReturned>%d</NumberReturned>\r\n",
                        numberReturned);
    chunk_writer.printf("<TotalMatches>%d</TotalMatches>\r\n", totalMatches);
    chunk_writer.printf("<UpdateID>%d</UpdateID>\r\n", updateID);

    chunk_writer.printf("</u:%s>\r\n", responseName);
    chunk_writer.println("</s:Body>");
    chunk_writer.println("</s:Envelope>");
    chunk_writer.end();
    return true;
  }

  /// generic SOAP reply template with placeholders: %1 = ActionResponse, %2 =
  /// payload tag, %3 = inner payload
  static const char* replyTemplate() {
    static const char* tpl =
        "\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n<u:%1 "
        "xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\"><%2>%3</"
        "%2>\n</u:%1>\n</s:Body>\n</"
        "s:Envelope>\n";
    return tpl;
  }

  // Control handler for ContentDirectory service
  static void contentDirectoryControlCB(HttpServer* server,
                                        const char* requestPath,
                                        HttpRequestHandlerLine* hl) {
    ActionRequest action;
    DLNADevice::parseActionRequest(server, requestPath, hl, action);

    // If a user-provided callback is registered, hand over the parsed
    // ActionRequest for custom handling. The callback is responsible for
    // writing the HTTP reply if it handles the action.
    MediaServer* ms = nullptr;
    if (hl && hl->context[0]) ms = (MediaServer*)hl->context[0];

    // process the requested action using instance method if available
    if (ms) {
      if (ms->processAction(action, *server)) return;
    }

    server->replyNotFound();
  }

  // simple connection manager control that replies OK
  static void connmgrControlCB(HttpServer* server, const char* requestPath,
                               HttpRequestHandlerLine* hl) {
    // Parse the incoming SOAP action
    ActionRequest action;
    DLNADevice::parseActionRequest(server, requestPath, hl, action);

    MediaServer* ms = nullptr;
    if (hl && hl->context[0]) ms = (MediaServer*)hl->context[0];

    if (!ms) {
      server->replyNotFound();
      return;
    }

    // Dispatch to instance methods for clarity and to allow overrides
    if (StrView(action.action).endsWith("GetProtocolInfo")) {
      ms->processActionGetProtocolInfo(action, *server);
      return;
    } else if (StrView(action.action).endsWith("GetCurrentConnectionIDs")) {
      ms->processActionGetCurrentConnectionIDs(action, *server);
      return;
    } else if (StrView(action.action).endsWith("GetCurrentConnectionInfo")) {
      ms->processActionGetCurrentConnectionInfo(action, *server);
      return;
    }

    // Unhandled actions: reply NotFound
    server->replyNotFound();
  }

  // helper to write text content with minimal XML-escaping for &, < and >
  static void writeEscapedText(Print& out, const char* s) {
    if (!s) return;
    for (const char* p = s; *p; ++p) {
      if (*p == '&')
        out.print("&amp;");
      else if (*p == '<')
        out.print("&lt;");
      else if (*p == '>')
        out.print("&gt;");
      else
        out.write((const uint8_t)*p);
    }
  }
};

}  // namespace tiny_dlna
