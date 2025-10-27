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

  /// Define the search capabilities: use csv
  void setSearchCapabilities(const char* caps) { g_search_capabiities = caps; }

  /// Define the sort capabilities: use csv
  void setSortCapabilities(const char* caps) { g_sort_capabilities = caps; }

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
  const char* g_sort_capabilities = "";

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
    if (StrView(action.action).equals("Browse")) {
      return processActionBrowse(action, server);
    } else if (action_str.equals("Search")) {
      return processActionSearch(action, server);
    } else if (action_str.equals("GetSearchCapabilities")) {
      return processActionGetSearchCapabilities(action, server);
    } else if (action_str.equals("GetSortCapabilities")) {
      return processActionGetSortCapabilities(action, server);
    } else if (action_str.equals("GetSystemUpdateID")) {
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
    DlnaLogger.log(DlnaLogLevel::Info, "processActionGetSearchCapabilities");
    Str reply_str{replyTemplate()};
    reply_str.replaceAll("%2", "ContentDirectory");
    reply_str.replaceAll("%1", "ActionResponse");
    reply_str.replaceAll("%3", g_search_capabiities);
    server.reply("text/xml", reply_str.c_str());
    return true;
  }

  bool processActionGetSortCapabilities(ActionRequest& action,
                                        HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "processActionGetSortCapabilities");
    Str reply_str{replyTemplate()};
    reply_str.replaceAll("%2", "ContentDirectory");
    reply_str.replaceAll("%1", "ActionResponse");
    reply_str.replaceAll("%3", g_sort_capabilities);
    server.reply("text/xml", reply_str.c_str());
    return true;
  }

  bool processActionGetSystemUpdateID(ActionRequest& action,
                                      HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Info, "processActionGetSystemUpdateID");
    Str reply_str{replyTemplate()};
    char update_id_str[80];
    sprintf(update_id_str, "%d", g_stream_updateID);
    reply_str.replaceAll("%2", "ContentDirectory");
    reply_str.replaceAll("%1", "ActionResponse");
    reply_str.replaceAll("%3", update_id_str);
    server.reply("text/xml", reply_str.c_str());
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
    ActionRequest action;
    DLNADevice::parseActionRequest(server, requestPath, hl, action);

    // If a user-provided callback is registered, hand over the parsed
    // ActionRequest for custom handling. The callback is responsible for
    // writing the HTTP reply if it handles the action.
    MediaServer* ms = nullptr;
    if (hl && hl->context[0]) ms = (MediaServer*)hl->context[0];

    // process the requested action using instance method if available
    if (ms) {
      ms->processAction(action, *server);
      return;
    }

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

  // streaming reply callback: writes the full SOAP envelope and escapes
  // DIDL tags while streaming the items from g_stream_items.
  static void streamReplyCallback(Print& out, const char* responseName) {
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
    xml.printNodeBeginNl(responseName, brAttrs, "u");

    // Result node: use callback to stream escaped DIDL
    xml.printNode("Result", [&]() -> size_t {
      buildEscapedDIDL(out, p_media_server->g_stream_get_data_cb,
                       p_media_server->g_stream_numberReturned);
      return 0;
    });

    // numeric fields
    xml.printNode("NumberReturned", p_media_server->g_stream_numberReturned);
    xml.printNode("TotalMatches", p_media_server->g_stream_totalMatches);
    xml.printNode("UpdateID", p_media_server->g_stream_updateID);

    xml.printNodeEnd(responseName, "u");
    xml.printNodeEnd("Body", "s");
    xml.printNodeEnd("Envelope", "s");
    p_media_server->g_stream_updateID++;
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
        if (!get_data_cb(i, item, p_media_server->g_stream_reference)) break;
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
