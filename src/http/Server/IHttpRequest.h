#pragma once

#include <functional>

#include "Client.h"
#include "HttpHeader.h"
#include "basic/Url.h"

class Print;

namespace tiny_dlna {

/**
 * @class IHttpRequest
 * @brief Abstract interface for HTTP client request functionality
 *
 * Defines the contract for implementing an HTTP client that can send various
 * types of HTTP requests (GET, POST, PUT, DELETE, etc.) and handle responses.
 * Supports UPnP-specific methods like SUBSCRIBE/UNSUBSCRIBE for event handling.
 */
class IHttpRequest {
 public:
  virtual ~IHttpRequest() = default;

  /// Set the client for HTTP requests
  virtual void setClient(Client& client) = 0;
  /// Set the host header for requests
  virtual void setHost(const char* host) = 0;
  /// Check if request interface is valid
  virtual operator bool() = 0;
  /// Check if connected to server
  virtual bool connected() = 0;
  /// Get number of bytes available to read
  virtual int available() = 0;
  /// Stop the connection
  virtual void stop() = 0;
  /// Send POST request with string data
  virtual int post(Url& url, const char* mime, const char* data,
                   int len = -1) = 0;
  /// Send POST request with callback writer
  virtual int post(Url& url, size_t len,
                   std::function<size_t(Print&, void*)> writer,
                   const char* mime = nullptr, void* ref = nullptr) = 0;
  /// Send NOTIFY request with callback writer
  virtual int notify(Url& url, std::function<size_t(Print&, void*)> writer,
                     const char* mime = nullptr, void* ref = nullptr) = 0;
  /// Send PUT request with string data
  virtual int put(Url& url, const char* mime, const char* data,
                  int len = -1) = 0;
  /// Send DELETE request
  virtual int del(Url& url, const char* mime = nullptr,
                  const char* data = nullptr, int len = -1) = 0;
  /// Send GET request
  virtual int get(Url& url, const char* acceptMime = nullptr,
                  const char* data = nullptr, int len = -1) = 0;
  /// Send HEAD request
  virtual int head(Url& url, const char* acceptMime = nullptr,
                   const char* data = nullptr, int len = -1) = 0;
  /// Send SUBSCRIBE request for UPnP events
  virtual int subscribe(Url& url) = 0;
  /// Send UNSUBSCRIBE request for UPnP events
  virtual int unsubscribe(Url& url, const char* sid) = 0;
  /// Read data from response
  virtual int read(uint8_t* str, int len) = 0;
  /// Read line from response
  virtual int readln(uint8_t* str, int len, bool incl_nl = true) = 0;
  /// Get reference to reply header
  virtual HttpReplyHeader& reply() = 0;
  /// Get reference to request header
  virtual HttpRequestHeader& request() = 0;
  /// Set User-Agent header
  virtual void setAgent(const char* agent) = 0;
  /// Set Connection header
  virtual void setConnection(const char* connection) = 0;
  /// Set Accept-Encoding header
  virtual void setAcceptsEncoding(const char* enc) = 0;
  /// Get pointer to client
  virtual Client* client() = 0;
  /// Set request timeout in milliseconds
  virtual void setTimeout(int ms) = 0;
};

}  // namespace tiny_dlna
