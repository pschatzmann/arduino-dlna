#pragma once

#include <stddef.h>

#include "Client.h"
#include "HttpHeader.h"
#include "basic/Str.h"
#include "basic/Url.h"

class IPAddress;
class Print;
class Stream;

namespace tiny_dlna {

class IHttpServer;
class HttpRequestHandlerLine;

using web_callback_fn = void (*)(IHttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* handlerLine);

class IHttpServer {
 public:
  virtual ~IHttpServer() = default;

  virtual IPAddress& localIP() = 0;
  virtual bool begin(int port, const char* ssid, const char* password) = 0;
  virtual bool begin(int port) = 0;
  virtual void end() = 0;
  virtual void rewrite(const char* from, const char* to) = 0;
  virtual void on(const char* url, TinyMethodID method, web_callback_fn fn,
                  void* ctx[] = nullptr, int ctxCount = 0) = 0;
  virtual void on(const char* url, TinyMethodID method, const char* mime,
                  web_callback_fn fn) = 0;
  virtual void on(const char* url, TinyMethodID method, const char* mime,
                  const char* result) = 0;
  virtual void on(const char* url, TinyMethodID method, const char* mime,
                  const uint8_t* data, int len) = 0;
  virtual void on(const char* url, TinyMethodID method, Url& redirect) = 0;
  virtual bool onRequest(const char* path) = 0;
  virtual void replyChunked(const char* contentType, Stream& inputStream,
                            int status = 200, const char* msg = SUCCESS) = 0;
  virtual void replyChunked(const char* contentType, int status = 200,
                            const char* msg = SUCCESS) = 0;
  virtual void reply(const char* contentType, Stream& inputStream, int size,
                     int status = 200, const char* msg = SUCCESS) = 0;
  virtual void reply(const char* contentType,
                     size_t (*callback)(Print& out, void* ref),
                     int status = 200, const char* msg = SUCCESS,
                     void* ref = nullptr) = 0;
  virtual void reply(const char* contentType, const char* str,
                     int status = 200, const char* msg = SUCCESS) = 0;
  virtual void reply(const char* contentType, const uint8_t* str, int len,
                     int status = 200, const char* msg = SUCCESS) = 0;
  virtual void replyOK() = 0;
  virtual void replyNotFound() = 0;
  virtual void replyError(int err,
                          const char* msg = "Internal Server Error") = 0;
  virtual HttpRequestHeader& requestHeader() = 0;
  virtual HttpReplyHeader& replyHeader() = 0;
  virtual void endClient() = 0;
  virtual void crlf() = 0;
  virtual void addHandler(HttpRequestHandlerLine* handlerLinePtr) = 0;
  virtual bool doLoop() = 0;
  virtual bool copy() = 0;
  virtual Client& client() = 0;
  virtual operator bool() = 0;
  virtual bool isActive() = 0;
  virtual const char* localHost() = 0;
  virtual void setNoConnectDelay(int delay) = 0;
  virtual Str contentStr() = 0;
  virtual void setReference(void* reference) = 0;
  virtual void* getReference() = 0;
};

}  // namespace tiny_dlna
