#pragma once

#include <functional>

#include "Client.h"
#include "HttpHeader.h"
#include "basic/Url.h"

class Print;

namespace tiny_dlna {

class IHttpRequest {
 public:
  virtual ~IHttpRequest() = default;

  virtual void setClient(Client& client) = 0;
  virtual void setHost(const char* host) = 0;
  virtual operator bool() = 0;
  virtual bool connected() = 0;
  virtual int available() = 0;
  virtual void stop() = 0;
  virtual int post(Url& url, const char* mime, const char* data,
                   int len = -1) = 0;
  virtual int post(Url& url, size_t len,
                   std::function<size_t(Print&, void*)> writer,
                   const char* mime = nullptr, void* ref = nullptr) = 0;
  virtual int notify(Url& url, std::function<size_t(Print&, void*)> writer,
                     const char* mime = nullptr, void* ref = nullptr) = 0;
  virtual int put(Url& url, const char* mime, const char* data,
                  int len = -1) = 0;
  virtual int del(Url& url, const char* mime = nullptr,
                  const char* data = nullptr, int len = -1) = 0;
  virtual int get(Url& url, const char* acceptMime = nullptr,
                  const char* data = nullptr, int len = -1) = 0;
  virtual int head(Url& url, const char* acceptMime = nullptr,
                   const char* data = nullptr, int len = -1) = 0;
  virtual int subscribe(Url& url) = 0;
  virtual int unsubscribe(Url& url, const char* sid) = 0;
  virtual int read(uint8_t* str, int len) = 0;
  virtual int readln(uint8_t* str, int len, bool incl_nl = true) = 0;
  virtual HttpReplyHeader& reply() = 0;
  virtual HttpRequestHeader& request() = 0;
  virtual void setAgent(const char* agent) = 0;
  virtual void setConnection(const char* connection) = 0;
  virtual void setAcceptsEncoding(const char* enc) = 0;
  virtual Client* client() = 0;
  virtual void setTimeout(int ms) = 0;
};

}  // namespace tiny_dlna
