// Minimal header-only Digital Media Server (MediaServer) device
#pragma once

#include "basic/Str.h"
#include "basic/StrPrint.h"
#include "dlna/DLNADeviceMgr.h"
#include "http/HttpServer.h"
// Generated SCPD headers
#include "ms_connmgr.h"
#include "ms_content_dir.h"
#include "dlna/xml/XMLPrinter.h"

namespace tiny_dlna {

/**
 * @brief Minimal Digital Media Server implementation
 *
 * This class implements a lightweight DLNA MediaServer device with a
 * ContentDirectory service (Browse) and a ConnectionManager service.
 * The implementation is intentionally compact and suitable as a starting
 * point for embedding content lists. It returns a single test item for
 * Browse requests.
 */
class MediaServer : public DLNADevice {
 public:
  /// Media item description used to build DIDL-Lite entries
  struct MediaItem {
    const char* id = nullptr;
    const char* parentID = "0";
    bool restricted = true;
    const char* title = nullptr;
    const char* res = nullptr;  // resource URL
    const char* mimeType = nullptr;
    // Additional optional metadata fields could be added here (duration,
    // creator...)
  };

  // Browse callback signature:
  // objectID, browseFlag, filter, startingIndex, requestedCount, sortCriteria,
  // results vector (to be filled by callback), numberReturned (out),
  // totalMatches (out), updateID (out)
  typedef void (*BrowseCallback)(const char* objectID, const char* browseFlag,
                                 const char* filter, int startingIndex,
                                 int requestedCount, const char* sortCriteria,
                                 Vector<MediaItem>& results,
                                 int& numberReturned, int& totalMatches,
                                 int& updateID);

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

  /// Sets the browse callback
  void setBrowseCallback(BrowseCallback cb) { browse_cb = cb; }

  /// Provides access to the http server
  HttpServer* getHttpServer() { return p_server; }  

 protected:
  const char* st = "urn:schemas-upnp-org:device:MediaServer:1";
  const char* usn = "uuid:media-server-0000-0000-0000-000000000001";
  // stored browse callback
  BrowseCallback browse_cb = nullptr;
  HttpServer* p_server = nullptr;

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

    Vector<MediaItem> results;
    int numberReturned = 0;
    int totalMatches = 0;
    int updateID = 1;
    if (ms && ms->browse_cb) {
      ms->browse_cb(obj.c_str(), flag.c_str(), filter.c_str(), startingIndex,
                    requestedCount, sort.c_str(), results, numberReturned,
                    totalMatches, updateID);
    } else {
      MediaItem it;
      it.id = "1";
      it.parentID = "0";
      it.restricted = true;
      it.title = "Test Item";
      it.res = "http://example.com/track.mp3";
      it.mimeType = "audio/mpeg";
      results.push_back(it);
      numberReturned = 1;
      totalMatches = 1;
      updateID = 1;
    }

    // Stream response via Print-callback and generate DIDL on-the-fly.
    // Set globals that the callback will read while running synchronously.
    g_stream_items = &results;
    g_stream_numberReturned = numberReturned;
    g_stream_totalMatches = totalMatches;
    g_stream_updateID = updateID;
    server->reply("text/xml", streamReplyCallback);
    // clear globals after reply returns
    g_stream_items = nullptr;
    g_stream_numberReturned = 0;
    g_stream_totalMatches = 0;
    g_stream_updateID = 0;
  }

  // Globals used by the streaming callback. These are set immediately
  // before calling server->reply(...) and cleared afterwards. This is
  // safe in the single-threaded server model.
  static inline Vector<MediaItem>* g_stream_items = nullptr;
  static inline int g_stream_numberReturned = 0;
  static inline int g_stream_totalMatches = 0;
  static inline int g_stream_updateID = 0;

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

  /// Print wrapper that escapes & < > while forwarding to an underlying Print
  struct EscapingPrint : public Print {
    Print& dest;
    EscapingPrint(Print& d) : dest(d) {}
    size_t write(uint8_t c) override {
      if (c == '&') return dest.print("&amp;");
      if (c == '<') return dest.print("&lt;");
      if (c == '>') return dest.print("&gt;");
      return dest.write(&c, 1);
    }
    size_t write(const uint8_t* buffer, size_t size) override {
      size_t r = 0;
      for (size_t i = 0; i < size; ++i) r += write(buffer[i]);
      return r;
    }
    int available() { return 0; }
  };

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
      if (g_stream_items)
        buildEscapedDIDL(out, *g_stream_items);
      else {
        out.print(
            "&lt;DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
            "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
            "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\"&gt;&lt;/"
            "DIDL-Lite&gt;");
      }
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
  static void buildEscapedDIDL(Print& out, Vector<MediaItem>& items) {
    // Use XMLPrinter to generate DIDL elements, but wrap the output with
    // EscapingPrint so '<' and '>' are emitted as escaped text into the
    // surrounding SOAP <Result> element.
    EscapingPrint esc(out);
    XMLPrinter didl(esc);

    const char* didlAttrs =
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
        "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\"";
    didl.printNodeBeginNl("DIDL-Lite", didlAttrs);

    for (auto it = items.begin(); it != items.end(); ++it) {
      const MediaItem& item = *it;
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

  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaServer::setupServices");

    auto contentDescCB = [](HttpServer* server, const char* requestPath,
                            HttpRequestHandlerLine* hl) {
      server->reply("text/xml", ms_content_dir_xml);
    };
    auto connDescCB = [](HttpServer* server, const char* requestPath,
                         HttpRequestHandlerLine* hl) {
      server->reply("text/xml", ms_conmgr_xml);
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
};

// service description strings are provided by the generated headers

}  // namespace tiny_dlna
