#pragma once

#include <functional>

#include "HttpChunkReader.h"
#include "HttpChunkWriter.h"
#include "HttpHeader.h"
#include "IHttpRequest.h"
#include "basic/StrPrint.h"
#ifdef ESP32
#include <WiFi.h>
#include <lwip/sockets.h>
#endif

namespace tiny_dlna {

/**
 * @brief Simple API to process get, put, post, del http requests
 * I tried to use Arduino HttpClient, but I  did not manage to extract the mime
 * type from streaming get requests.
 *
 * The functionality is based on the Arduino Client class. The template
 * parameter defines the concrete client that will be managed (e.g.
 * WiFiClient); callers must now provide it explicitly.
 *
 * @tparam ClientType concrete Arduino client type that implements the
 *         `Client` interface (e.g. `WiFiClient`, `EthernetClient`).
 */

template <typename ClientType>
class HttpRequest : public IHttpRequest {
 public:
  /// Default constructor.
  HttpRequest() {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpRequest (default client)");
    setClient(default_client);
  }

  /// Constructor with specified client.
  explicit HttpRequest(ClientType& client) {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpRequest");
    setClient(client);
  }

  /// Destructor.
  ~HttpRequest() override { stop(); }

  /// Sets the client to be used for the requests.
  void setClient(Client& client) override { this->client_ptr = &client; }

  /// Sets the host name for the requests.
  void setHost(const char* host) override {
    DlnaLogger.log(DlnaLogLevel::Info, "HttpRequest::setHost: ", host);
    this->host_name = host;
  }

  /// Checks if the request is valid.
  operator bool() override {
    return client_ptr != nullptr && static_cast<bool>(*client_ptr);
  }

  /// Checks if connected to the server.
  bool connected() override { return client_ptr->connected(); }

  /// Returns the number of available bytes to read.
  int available() override {
    if (reply_header.isChunked()) {
      return chunk_reader.available();
    }
    return client_ptr->available();
  }

  /// Stops the connection.
  void stop() override {
    DlnaLogger.log(DlnaLogLevel::Info, "HttpRequest::stop");
    if (client_ptr != nullptr) {
      client_ptr->stop();
      // delay(300);
    }
  }

  /// Sends a POST request with data.
  int post(Url& url, const char* mime, const char* data,
           int len = -1) override {
    DlnaLogger.log(DlnaLogLevel::Info, "post %s", url.url());
    return process(T_POST, url, mime, data, len);
  }

  /// Sends a POST request with streaming body.
  int post(Url& url, size_t len, std::function<size_t(Print&, void*)> writer,
           const char* mime = nullptr, void* ref = nullptr) override {
    return process(T_POST, url, len, writer, mime, ref);
  }

  /// Sends a NOTIFY request with streaming body.
  int notify(Url& url, std::function<size_t(Print&, void*)> writer,
             const char* mime = nullptr, void* ref = nullptr) override {
    NullPrint nop;
    int len = writer(nop, ref);
    return process(T_NOTIFY, url, len, writer, mime, ref);
  }

  /// Sends a PUT request.
  int put(Url& url, const char* mime, const char* data, int len = -1) override {
    DlnaLogger.log(DlnaLogLevel::Info, "put %s", url.url());
    return process(T_PUT, url, mime, data, len);
  }

  /// Sends a DELETE request.
  int del(Url& url, const char* mime = nullptr, const char* data = nullptr,
          int len = -1) override {
    DlnaLogger.log(DlnaLogLevel::Info, "del %s", url.url());
    return process(T_DELETE, url, mime, data, len);
  }

  /// Sends a GET request.
  int get(Url& url, const char* acceptMime = nullptr,
          const char* data = nullptr, int len = -1) override {
    DlnaLogger.log(DlnaLogLevel::Info, "get %s", str(url.url()));
    this->accept = acceptMime;
    return process(T_GET, url, nullptr, data, len);
  }

  /// Sends a HEAD request.
  int head(Url& url, const char* acceptMime = nullptr,
           const char* data = nullptr, int len = -1) override {
    DlnaLogger.log(DlnaLogLevel::Info, "head %s", url.url());
    this->accept = acceptMime;
    return process(T_HEAD, url, nullptr, data, len);
  }

  /// Sends a SUBSCRIBE request.
  int subscribe(Url& url) override {
    DlnaLogger.log(DlnaLogLevel::Info, "%s %s", methods[T_SUBSCRIBE], url.path());
    return process(T_SUBSCRIBE, url, nullptr, nullptr, 0);
  }

  /// Sends an UNSUBSCRIBE request.
  int unsubscribe(Url& url, const char* sid) override {
    DlnaLogger.log(DlnaLogLevel::Info, "%s %s (SID=%s)", methods[T_UNSUBSCRIBE],
                   url.path(), sid);
    if (sid != nullptr) {
      request_header.put("SID", sid);
    }
    return process(T_UNSUBSCRIBE, url, nullptr, nullptr, 0);
  }

  /// Reads reply data.
  int read(uint8_t* str, int len) override {
    if (reply_header.isChunked()) {
      return chunk_reader.read(*client_ptr, str, len);
    } else {
      return client_ptr->read(str, len);
    }
  }

  /// Reads reply data up to the next new line.
  int readln(uint8_t* str, int len, bool incl_nl = true) override {
    if (reply_header.isChunked()) {
      return chunk_reader.readln(*client_ptr, str, len);
    } else {
      return chunk_reader.readlnInternal(*client_ptr, str, len, incl_nl);
    }
  }

  /// Returns the reply header.
  HttpReplyHeader& reply() override { return reply_header; }

  /// Returns the request header.
  HttpRequestHeader& request() override { return request_header; }

  /// Sets the user agent.
  void setAgent(const char* agent) override { this->agent = agent; }

  /// Sets the connection type.
  void setConnection(const char* connection) override {
    this->connection = connection;
  }

  /// Sets the accepted encodings.
  void setAcceptsEncoding(const char* enc) override {
    this->accept_encoding = enc;
  }

  /// Returns the client pointer.
  Client* client() override { return client_ptr; }

  /// Sets the timeout.
  void setTimeout(int ms) override { client_ptr->setTimeout(ms); }

  bool isKeepAlive() { return StrView(connection).equals(CON_KEEP_ALIVE); }

 protected:
  ClientType default_client;
  Client* client_ptr = nullptr;
  Url url;
  HttpRequestHeader request_header;
  HttpReplyHeader reply_header;
  HttpChunkReader chunk_reader{reply_header};
  Str host_name;
  const char* agent = nullptr;
  const char* connection = CON_CLOSE;
  const char* accept = ACCEPT_ALL;
  const char* accept_encoding = nullptr;

  /// Returns an empty string if input is null, otherwise returns the input.
  const char* str(const char* in) { return in == nullptr ? "" : in; }

  /// opens a connection to the indicated host
  virtual int connect(const char* ip, uint16_t port) {
    this->client_ptr->setTimeout(DLNA_HTTP_REQUEST_TIMEOUT_MS);
    if (!isKeepAlive() && this->client_ptr->connected()) {
      stop();
    }
#ifdef ESP32
    // clear input buffer
    static_cast<ClientType*>(client_ptr)->clear();
    // static_cast<ClientType*>(client_ptr)
    //     ->setConnectionTimeout(DLNA_HTTP_REQUEST_TIMEOUT_MS);
    static_cast<ClientType*>(client_ptr)->setNoDelay(true);
    int enable = 1;
    static_cast<ClientType*>(client_ptr)
        ->setSocketOption(SOL_SOCKET, SO_REUSEADDR, &enable,
                          sizeof(enable));  // Address reuse
#endif
    DlnaLogger.log(DlnaLogLevel::Info, "HttpRequest::connect %s:%d", ip, port);
    uint64_t end = millis() + client_ptr->getTimeout();
    bool rc = false;
    for (int j = 0; j < 3; j++) {
      rc = this->client_ptr->connect(ip, port);
      if (rc) break;
      delay(200);
    }
    if (!connected()) {
      DlnaLogger.log(
          DlnaLogLevel::Error, "Connected: %s (rc=%d) with timeout %ld",
          connected() ? "true" : "false", rc, client_ptr->getTimeout());
    } else {
      DlnaLogger.log(
          DlnaLogLevel::Debug, "Connected: %s (rc=%d) with timeout %ld",
          connected() ? "true" : "false", rc, client_ptr->getTimeout());
    }
    return rc;
  }

  /// sends request and reads the reply_header from the server
  virtual int process(TinyMethodID action, Url& url, const char* mime,
                      const char* data, int len = -1) {
    if (!connected()) {
      DlnaLogger.log(DlnaLogLevel::Info, "Connecting to host %s port %d",
                     url.host(), url.port());

      connect(url.host(), url.port());
    }

    if (!connected()) {
      DlnaLogger.log(DlnaLogLevel::Info, "Connected: %s",
                     connected() ? "true" : "false");
      return -1;
    }

    if (host_name.isEmpty()) {
      host_name = url.host();
      host_name.add(":");
      host_name.add(url.port());
    }

    request_header.setValues(action, url.path());
    if (len == -1 && data != nullptr) {
      len = strlen(data);
    }
    if (len > 0) {
      request_header.put(CONTENT_LENGTH, len);
    }
    if (!host_name.isEmpty()) {
      request_header.put(HOST_C, host_name.c_str());
    }
    if (agent != nullptr) {
      request_header.put(USER_AGENT, agent);
    }
    if (accept_encoding != nullptr) {
      request_header.put(ACCEPT_ENCODING, accept_encoding);
    }
    if (mime != nullptr) {
      request_header.put(CONTENT_TYPE, mime);
    }

    request_header.put(CONNECTION, connection);
    request_header.put(ACCEPT, accept);

    request_header.write(*client_ptr);

    if (len > 0) {
      DlnaLogger.log(DlnaLogLevel::Info, "process - writing data: %d bytes",
                     len);
      client_ptr->write((const uint8_t*)data, len);
    }

    reply_header.read(*client_ptr);

    // if we use chunked tranfer we need to read the first chunked length
    if (reply_header.isChunked()) {
      chunk_reader.open(*client_ptr);
    };

    return reply_header.statusCode();
  }

  /// Shared implementation for streaming requests. Both POST and
  /// NOTIFY (and potentially others) can use this to avoid code duplication.
  virtual int process(TinyMethodID method, Url& url, size_t len,
                      std::function<size_t(Print&, void*)> writer,
                      const char* mime = nullptr, void* ref = nullptr) {
    DlnaLogger.log(DlnaLogLevel::Info, "%s %s", methods[method], url.url());

    if (!connected()) {
      connect(url.host(), url.port());
    }

    if (!connected()) {
      DlnaLogger.log(DlnaLogLevel::Info, "Connected: %s",
                     connected() ? "true" : "false");
      return -1;
    }

    if (host_name.isEmpty()) {
      host_name = url.host();
      host_name.add(":");
      host_name.add(url.port());
    }

    // prepare request header
    request_header.setValues(method, url.path());
    if (!host_name.isEmpty()) {
      request_header.put(HOST_C, host_name.c_str());
    }
    if (agent != nullptr) request_header.put(USER_AGENT, agent);
    if (accept_encoding != nullptr)
      request_header.put(ACCEPT_ENCODING, accept_encoding);
    if (mime != nullptr) request_header.put(CONTENT_TYPE, mime);
    if (len > 0) request_header.put(CONTENT_LENGTH, len);

    request_header.put(CONNECTION, connection);
    request_header.put(ACCEPT, accept);

    // write request header to client
    request_header.write(*client_ptr);

    // write callback (writer returns number of bytes written)
    if (writer) {
      size_t written = writer(*client_ptr, ref);
#if DLNA_LOG_XML
      writer(Serial, ref);
#endif
      if (written != len) {
        DlnaLogger.log(DlnaLogLevel::Error,
                       "HttpRequest wrote %d bytes: expected %d", written, len);
      }
#if DLNA_CHECK_XML_LENGTH
      StrPrint test;
      size_t test_len = writer(test, ref);
      if (strlen(test.c_str()) != len) {
        DlnaLogger.log(
            DlnaLogLevel::Error,
            "HttpRequest test wrote %d bytes: expected %d / strlen: %d",
            test_len, len, strlen(test.c_str()));
      }
#endif
    }

    // read reply header and prepare chunk reader if needed
    reply_header.read(*client_ptr);
    if (reply_header.isChunked()) {
      chunk_reader.open(*client_ptr);
    }

    return reply_header.statusCode();
  }
};

}  // namespace tiny_dlna

// using ma = tiny_dlna::HttpRequest;
