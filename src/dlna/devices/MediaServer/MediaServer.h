// Minimal header-only Digital Media Server (MediaServer) device
#pragma once

#include "MediaItem.h"
#include "basic/EscapingPrint.h"
#include "basic/Str.h"
#include "basic/StrPrint.h"
#include "dlna/DLNADeviceMgr.h"
#include "dlna/devices/MediaServer/MediaItem.h"
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
class MediaServer : public DLNADevice {
 public:
  // MediaItem is now defined in MediaItem.h

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

  // (No default GetDataCallback — user must register one to serve items.)

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

  void setupServices(HttpServer& server, IUDPService& udp) {
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

 protected:
  const char* st = "urn:schemas-upnp-org:device:MediaServer:1";
  const char* usn = "uuid:media-server-0000-0000-0000-000000000001";
  // stored callbacks
  PrepareDataCallback prepare_data_cb = nullptr;
  GetDataCallback get_data_cb = nullptr;
  HttpServer* p_server = nullptr;
  void* reference_ = nullptr;
  // Globals used by the streaming callback. These are set immediately
  // before calling server->reply(...) and cleared afterwards. This is
  // safe in the single-threaded server model.
  static inline GetDataCallback g_stream_get_data_cb = nullptr;
  static inline int g_stream_numberReturned = 0;
  static inline int g_stream_totalMatches = 0;
  static inline int g_stream_updateID = 0;
  static inline void* g_stream_reference = nullptr;

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

  /// generic SOAP reply template with placeholders: %1 = ActionResponse, %2 =
  /// ServiceName, %3 = inner payload
  static const char* replyTemplate() {
    static const char* tpl =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:%1 "
        "xmlns:u=\"urn:schemas-upnp-org:service:%2:1\">%3</u:%1></s:Body></"
        "s:Envelope>";
    return tpl;
  }

  // Control handler for ContentDirectory service
  static void contentDirectoryControlCB(HttpServer* server,
                                        const char* requestPath,
                                        HttpRequestHandlerLine* hl) {
    Str soap = server->contentStr();
    DlnaLogger.log(DlnaLogLevel::Info, "ContentDirectory Control: %s",
                   soap.c_str());
    Str reply_str{replyTemplate()};
    reply_str.replaceAll("%2", "ContentDirectory");

    // Handle only Browse for now
    if (soap.indexOf("Browse") < 0) {
      reply_str.replaceAll("%1", "ActionResponse");
      reply_str.replaceAll("%3", "");
      server->reply("text/xml", reply_str.c_str());
      return;
    }

    // Extract common arguments
    auto extractArg = [&](const char* tag) -> Str {
      Str result;
      int pos = soap.indexOf(tag);
      if (pos >= 0) {
        int start = soap.indexOf('>', pos);
        if (start >= 0) {
          start += 1;
          int end = soap.indexOf("</", start);
          if (end > start) result = soap.substring(start, end);
        }
      }
      return result;
    };

    Str obj = extractArg("ObjectID");
    Str flag = extractArg("BrowseFlag");
    Str filter = extractArg("Filter");
    Str startIdx = extractArg("StartingIndex");
    Str reqCount = extractArg("RequestedCount");
    Str sort = extractArg("SortCriteria");

    int startingIndex = startIdx.isEmpty() ? 0 : startIdx.toInt();
    int requestedCount = reqCount.isEmpty() ? 0 : reqCount.toInt();

    // collect results using callback or default
    MediaServer* ms = nullptr;
    if (hl && hl->context[0]) ms = (MediaServer*)hl->context[0];

    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = 1;
    if (ms && ms->prepare_data_cb) {
      ms->prepare_data_cb(obj.c_str(), flag.c_str(), filter.c_str(),
                          startingIndex, requestedCount, sort.c_str(),
                          numberReturned, totalMatches, updateID,
                          ms->reference_);
    } else {
      numberReturned = 0;
      totalMatches = 0;
      updateID = 0;
    }

    // Stream response via Print-callback and generate DIDL on-the-fly.
    // Set globals that the callback will read while running synchronously.
    g_stream_numberReturned = numberReturned;
    g_stream_totalMatches = totalMatches;
    g_stream_updateID = updateID;
    g_stream_get_data_cb = ms ? ms->get_data_cb : nullptr;
    g_stream_reference = ms ? ms->reference_ : nullptr;
    server->reply("text/xml", streamReplyCallback);
    // clear globals after reply returns
    g_stream_numberReturned = 0;
    g_stream_totalMatches = 0;
    g_stream_updateID = 0;
    g_stream_get_data_cb = nullptr;
    g_stream_reference = nullptr;
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

  // streaming reply callback: writes the full SOAP envelope and escapes
  // DIDL tags while streaming the items from g_stream_items.
  static void streamReplyCallback(Print& out) {
    XMLPrinter xml(out);
    // Envelope start with namespace and encoding attribute
    const char* envAttrs =
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"";
    xml.printNodeBegin("Envelope", envAttrs, "s");
    xml.printNodeBeginNl("Body", nullptr, "s");

    // BrowseResponse with its namespace declaration
    const char* brAttrs =
        "xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\"";
    xml.printNodeBeginNl("BrowseResponse", brAttrs, "u");

    // Result node: use callback to stream escaped DIDL
    xml.printNode("Result", [&]() -> size_t {
      buildEscapedDIDL(out, g_stream_get_data_cb, g_stream_numberReturned);
      return 0;
    });

    // numeric fields
    xml.printNode("NumberReturned", g_stream_numberReturned);
    xml.printNode("TotalMatches", g_stream_totalMatches);
    xml.printNode("UpdateID", g_stream_updateID);

    xml.printNodeEnd("BrowseResponse", "u");
    xml.printNodeEnd("Body", "s");
    xml.printNodeEnd("Envelope", "s");
  }

  // helper: stream DIDL-Lite (escaped) for the provided items to the Print
  static void buildEscapedDIDL(Print& out, GetDataCallback get_data_cb,
                               int count) {
    EscapingPrint esc(out);
    XMLPrinter didl(esc);

    const char* didlAttrs =
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
        "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\"";
    didl.printNodeBeginNl("DIDL-Lite", didlAttrs);

    if (get_data_cb) {
      for (int i = 0; i < count; ++i) {
        MediaItem item;
        if (!get_data_cb(i, item, g_stream_reference)) break;
        char itemAttr[256];
        snprintf(itemAttr, sizeof(itemAttr),
                 "id=\"%s\" parentID=\"%s\" restricted=\"%d\"",
                 item.id ? item.id : "", item.parentID ? item.parentID : "0",
                 item.restricted ? 1 : 0);
        didl.printNodeBeginNl("item", itemAttr);

        // title
        didl.printNode("dc:title", item.title ? item.title : "");

        // res with optional protocolInfo attribute
        if (item.mimeType) {
          char resAttr[128];
          snprintf(resAttr, sizeof(resAttr), "protocolInfo=\"%s\"",
                   item.mimeType);
          didl.printNode("res", item.res ? item.res : "", resAttr);
        } else {
          didl.printNode("res", item.res ? item.res : "");
        }

        didl.printNodeEnd("item");
      }
    }  // end if get_data_cb
    didl.printNodeEnd("DIDL-Lite");
  }

  // simple connection manager control that replies OK
  static void connmgrControlCB(HttpServer* server, const char* requestPath,
                               HttpRequestHandlerLine* hl) {
    // For simplicity we'll return an empty ActionResponse
    Str reply_str{replyTemplate()};
    reply_str.replaceAll("%2", "ConnectionManager");
    reply_str.replaceAll("%1", "ActionResponse");
    reply_str.replaceAll("%3", "");
    server->reply("text/xml", reply_str.c_str());
  }
};

}  // namespace tiny_dlna
