#pragma once

#include <stdlib.h>

#include "Client.h"
#include "HardwareSerial.h"
#include "HttpChunkWriter.h"
#include "HttpClientHandler.h"
#include "HttpHeader.h"
#include "HttpRequestHandlerLine.h"
#include "HttpRequestRewrite.h"
#include "IHttpServer.h"
#include "Server.h"
#include "basic/IPAddressAndPort.h"  // for toStr
#include "basic/List.h"
#include "basic/StrPrint.h"
#include "concurrency/ListLockFree.h"

namespace tiny_dlna {

/**
 * @brief Header-only HTTP server wrapper that registers callback handlers.
 *
 * This server is templated so you can provide the concrete Arduino
 * networking classes that back the transport. `ClientType` is the socket
 * implementation accepted from the server (for example `WiFiClient` or
 * `EthernetClient`) and `ServerType` is the listener type (for example
 * `WiFiServer`). Default template arguments keep the legacy WiFi behaviour
 * intact, while custom transports can swap in their own client/server types.
 *
 * @tparam ClientType Arduino client class accepted from the listener
 *         (e.g. `WiFiClient`, `EthernetClient`).
 * @tparam ServerType Arduino server/listener class that produces
 *         `ClientType` instances (e.g. `WiFiServer`, `EthernetServer`).
 */
template <typename ClientType, typename ServerType>
class HttpServer : public IHttpServer {
 public:
  HttpServer(ServerType& server, int bufferSize = 1024) {
    DlnaLogger.log(DlnaLogLevel::Info, "HttpServer");
    this->server_ptr = &server;
    client_handler.resize(bufferSize);
  }

  ~HttpServer() override {
    DlnaLogger.log(DlnaLogLevel::Info, "~HttpServer");
    handler_collection.clear();
    client_handler.requestHeader().clear(false);
    client_handler.replyHeader().clear(false);
    rewrite_collection.clear();
  }

  /// Provides the local ip address
  IPAddress& localIP() override {
    static IPAddress address;
    address = WiFi.localIP();
    return address;
  }

  /// Starts the server
  bool begin() override {
    DlnaLogger.log(DlnaLogLevel::Info, "HttpServer begin");
    is_active = true;
    server_ptr->begin();
    return true;
  }

  /// stops the server_ptr
  void end() override {
    DlnaLogger.log(DlnaLogLevel::Info, "HttpServer %s", "stop");
    is_active = false;
  }

  /// adds a rewrite rule
  void rewrite(const char* from, const char* to) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Rewriting %s to %s", from, to);
    HttpRequestRewrite* line = new HttpRequestRewrite(from, to);
    rewrite_collection.push_back(line);
  }

  /// register a generic handler
  void on(const char* url, TinyMethodID method, web_callback_fn fn,
          void* ctx[] = nullptr, int ctxCount = 0) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Serving at %s", url);
    HttpRequestHandlerLine* hl = new HttpRequestHandlerLine(ctxCount);
    hl->path = url;
    hl->fn = fn;
    hl->method = method;
    // hl->context = ctx;
    memmove(hl->context, ctx, ctxCount * sizeof(void*));
    hl->contextCount = ctxCount;
    addHandler(hl);
  }

  /// register a handler with mime
  void on(const char* url, TinyMethodID method, const char* mime,
          web_callback_fn fn) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Serving at %s", url);
    HttpRequestHandlerLine* hl = new HttpRequestHandlerLine();
    hl->path = url;
    hl->fn = fn;
    hl->method = method;
    hl->mime = mime;
    addHandler(hl);
  }

  /// register a handler which provides the indicated string
  void on(const char* url, TinyMethodID method, const char* mime,
          const char* result) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Serving at %s", url);

    auto lambda = [](IClientHandler& handler, IHttpServer*, const char*,
                     HttpRequestHandlerLine* hl) {
      DlnaLogger.log(DlnaLogLevel::Info, "on-strings %s", "lambda");
      if (hl->contextCount < 2) {
        DlnaLogger.log(DlnaLogLevel::Error, "The context is not available");
        return;
      }
      const char* mime = (const char*)hl->context[0];
      const char* msg = (const char*)hl->context[1];
      handler.reply(mime, msg, 200);
    };
    HttpRequestHandlerLine* hl = new HttpRequestHandlerLine(2);
    hl->context[0] = (void*)mime;
    hl->context[1] = (void*)result;
    hl->path = url;
    hl->fn = lambda;
    hl->method = method;
    addHandler(hl);
  }

  /// register a handler which provides the indicated string
  void on(const char* url, TinyMethodID method, const char* mime,
          const uint8_t* data, int len) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Serving at %s", url);

    auto lambda = [](IClientHandler& handler, IHttpServer*, const char*,
                     HttpRequestHandlerLine* hl) {
      DlnaLogger.log(DlnaLogLevel::Info, "on-strings %s", "lambda");
      if (hl->contextCount < 3) {
        DlnaLogger.log(DlnaLogLevel::Error, "The context is not available");
        return;
      }
      const char* mime = static_cast<char*>(hl->context[0]);
      const uint8_t* data = static_cast<uint8_t*>(hl->context[1]);
      int* p_len = (int*)hl->context[2];
      int len = *p_len;
      DlnaLogger.log(DlnaLogLevel::Debug, "Mime %d - Len: %d", mime, len);
      handler.reply(mime, data, len, 200);
    };
    HttpRequestHandlerLine* hl = new HttpRequestHandlerLine(3);
    hl->context[0] = (void*)mime;
    hl->context[1] = (void*)data;
    hl->context[2] = new int(len);
    hl->path = url;
    hl->fn = lambda;
    hl->method = method;
    addHandler(hl);
  }

  /// register a redirection
  void on(const char* url, TinyMethodID method, Url& redirect) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Serving at %s", url);
    auto lambda = [](IClientHandler& handler, IHttpServer*, const char*,
                     HttpRequestHandlerLine* hl) {
      if (hl->contextCount < 1) {
        DlnaLogger.log(DlnaLogLevel::Error, "The context is not available");
        return;
      }
      DlnaLogger.log(DlnaLogLevel::Info, "on-redirect %s", "lambda");
      HttpReplyHeader reply_header;
      Url* url = static_cast<Url*>(hl->context[0]);
      reply_header.setValues(301, "Moved");
      reply_header.put(LOCATION, url->url());
      reply_header.put("X-Forwarded-Host", (const char*)hl->context[1]);
      reply_header.write(*handler.client());
      handler.endClient();
    };

    HttpRequestHandlerLine* hl = new HttpRequestHandlerLine(2);
    const char* lh = localHost();
    hl->context[0] = new Url(redirect);
    hl->context[1] = (void*)lh;
    hl->path = url;
    hl->fn = lambda;
    hl->method = method;
    addHandler(hl);
  }

  /// generic handler - you can overwrite this method to provide your specifc
  /// processing logic
  bool onRequest(const char* path) override {
    DlnaLogger.log(DlnaLogLevel::Info, "Serving at %s", path);

    bool result = false;
    // check in registered handlers
    StrView pathStr = StrView(path);
    pathStr.replace("//", "/");  // TODO investiage why we get //
    for (auto handler_line_ptr : handler_collection) {
      DlnaLogger.log(DlnaLogLevel::Debug, "onRequest: %s %s vs: %s %s %s", path,
                     methods[client_handler.requestHeader().method()],
                     nullstr(handler_line_ptr->path.c_str()),
                     methods[handler_line_ptr->method],
                     nullstr(handler_line_ptr->mime));

      if (pathStr.matches(handler_line_ptr->path.c_str()) &&
          client_handler.requestHeader().method() == handler_line_ptr->method &&
          matchesMime(nullstr(handler_line_ptr->mime),
                      nullstr(client_handler.requestHeader().accept()))) {
        // call registed handler function
        DlnaLogger.log(DlnaLogLevel::Debug, "onRequest %s", "->found",
                       nullstr(handler_line_ptr->path.c_str()));
        handler_line_ptr->fn(client_handler, this, path, handler_line_ptr);
        result = true;
        break;
      }
    }

    if (!result) {
      DlnaLogger.log(DlnaLogLevel::Error, "Request %s not available", path);
    }

    return result;
  }

  /// adds a new handler
  void addHandler(HttpRequestHandlerLine* handlerLinePtr) {
    handler_collection.push_back(handlerLinePtr);
  }

  /// Legacy method: same as copy();
  bool doLoop() override { return copy(); }

  /// Call this method from your loop!
  bool copy() override {
    if (!is_active) {
      delay(no_connect_delay);
      return false;
    }

    // Accept new client and add to list if connected
    ClientType client = server_ptr->accept();
    if (client.connected()) {
      DlnaLogger.log(DlnaLogLevel::Info, "copy: accepted new client");
      client.setTimeout(DLNA_HTTP_READ_TIMEOUT_MS);
#ifdef ESP32    
      client.setNoDelay(true);  // disables Nagle
#endif
      open_clients.push_back(client);
    }

    // Stop when nothing to process
    if (open_clients.empty()) {
      // delay(no_connect_delay);
      return false;
    }

    if (current_client_iterator == open_clients.end()) {
      current_client_iterator = open_clients.begin();
      if (current_client_iterator == open_clients.end()) {
        delay(no_connect_delay);
        return false;
      }
    }

    ClientType* p_client = &(*current_client_iterator);
    if (!p_client->connected()) {
      DlnaLogger.log(DlnaLogLevel::Debug, "copy: removing disconnected client");
      auto to_erase = current_client_iterator;
      ++current_client_iterator;
      open_clients.erase(to_erase);
      return false;
    }
    if (p_client->available() == 0) {
      DlnaLogger.log(DlnaLogLevel::Debug,
                     "copy: no data available from client");
      ++current_client_iterator;
      if (current_client_iterator == open_clients.end() &&
          !open_clients.empty()) {
        current_client_iterator = open_clients.begin();
      }
      return false;
    }
    processRequest(p_client);
    ++current_client_iterator;
    if (current_client_iterator == open_clients.end() &&
        !open_clients.empty()) {
      current_client_iterator = open_clients.begin();
    }

    // cleanup clients
    removeClosedClients();
    return true;
  }

  /// Provides true if the server has been started
  operator bool() override { return is_active; }

  bool isActive() override { return is_active; }

  /// Determines the local ip address
  const char* localHost() override {
    if (local_host == nullptr) {
      local_host = toStr(WiFi.localIP());
    }
    return local_host;
  }

  void setNoConnectDelay(int delay) override { no_connect_delay = delay; }

  // /// converts the client content to a string
  // Str contentStr(ClientType* client) {
  //   uint8_t buffer[1024];
  //   Str result;
  //   while (client->available()) {
  //     int len = client->readBytes(buffer, sizeof(buffer));
  //     result.add((const uint8_t*)buffer, len);
  //   }
  //   result.add("\0");
  //   return result;
  // }

  // const char* urlPath() { return client_handler.requestHeader().urlPath(); }

  /// Definesa reference/context object
  void setReference(void* reference) override { ref = reference; }

  /// Provides access to a reference/context object
  void* getReference() override { return ref; }

 protected:
  // data
  List<HttpRequestHandlerLine*> handler_collection;
  List<HttpRequestRewrite*> rewrite_collection;
  ListLockFree<ClientType> open_clients;
  HttpClientHandler<ClientType> client_handler;
  typename ListLockFree<ClientType>::Iterator current_client_iterator =
      open_clients.begin();
  ServerType* server_ptr = nullptr;
  bool is_active;
  const char* local_host = nullptr;
  int no_connect_delay = 5;
  void* ref = nullptr;

  /* Remove all closed/disconnected clients from the open_clients list.
   * This also keeps `current_client_iterator` valid by advancing it to
   * the next element when the erased element equals the current iterator.
   */
  void removeClosedClients() {
    auto it = open_clients.begin();
    while (it != open_clients.end()) {
      if ((!(*it).connected())) {
        auto to_erase = it;
        ++it;
        open_clients.erase(to_erase);
        if (current_client_iterator == to_erase) {
          current_client_iterator = it;
        }
      } else {
        ++it;
      }
    }
    if (open_clients.empty()) {
      current_client_iterator = open_clients.begin();
    }
  }

  // Converts null to an empty string
  const char* nullstr(const char* in) { return in == nullptr ? "" : in; }

  // process a full request and send the reply
  void processRequest(ClientType* p_client) {
    DlnaLogger.log(DlnaLogLevel::Info, "processRequest");
    client_handler.setClient(p_client);
    client_handler.readHttpHeader();
    const char* path = client_handler.requestHeader().urlPath();
    path = resolveRewrite(path);
    bool processed = onRequest(path);
    if (!processed) {
      client_handler.replyNotFound();
    }
  }
  /// determiens the potentially rewritten url which should be used for the
  /// further processing
  const char* resolveRewrite(const char* from) {
    for (auto i = rewrite_collection.begin(); i != rewrite_collection.end();
         ++i) {
      HttpRequestRewrite* rewrite = (*i);
      if (rewrite->from.matches(from)) {
        return rewrite->to.c_str();
      }
    }
    return from;
  }

  /// compares mime of handler with mime of request: provides true if they match
  /// or one is null (=any value)
  bool matchesMime(const char* handler_mime, const char* request_mime) {
    DlnaLogger.log(DlnaLogLevel::Debug, "matchesMime: %s vs %s", handler_mime,
                   request_mime);
    if (StrView(handler_mime).isEmpty() || StrView(request_mime).isEmpty()) {
      return true;
    }
    bool result = StrView(request_mime).contains(handler_mime);
    return result;
  }
};

using WiFiHttpServer = HttpServer<WiFiClient, WiFiServer>;

}  // namespace tiny_dlna
