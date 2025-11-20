#pragma once

#include <stddef.h>
#include <string.h>

#include "HttpChunkWriter.h"
#include "HttpHeader.h"
#include "IHttpServer.h"
#include "basic/RetryPrint.h"
#include "basic/StrPrint.h"

namespace tiny_dlna {

/**
 * @brief Handles HTTP client connections and responses for the DLNA HTTP
 * server.
 *
 * This class encapsulates all logic for managing a single HTTP client
 * connection, including reading requests, writing replies (including chunked
 * and callback-based), and managing the internal buffer. It is designed to be
 * used as a handler within the HttpServer class template, and supports both
 * pointer and reference-based client assignment. All HTTP response methods are
 * provided, including error and not-found replies, and the handler is
 * responsible for managing the client lifecycle.
 *
 * @tparam ClientT The concrete client type (e.g., WiFiClient, EthernetClient)
 * used for network I/O.
 */
template <typename ClientT>
class HttpClientHandler : public IClientHandler {
 public:
  // Default constructor for use in HttpServer
  HttpClientHandler() = default;

  HttpClientHandler(ClientT& client) { setClient(&client); }
  HttpClientHandler(ClientT* client) { setClient(client); }
  virtual ~HttpClientHandler() = default;

  void setClient(ClientT* client) { p_client = client; }

  /// Reads the http header info from the client
  void readHttpHeader() {
    if (!p_client) return;
    Client& client = *p_client;
    int count = 50;
    while (client.available() == 0) {
      delay(20);
      if (count-- <= 0) {
        break;
      }
    }
    request_header.read(*this->p_client);
    // provide reply with empty header
    reply_header.clear();
  }

  // Helper for writing status and message to the reply (uses p_client)
  void reply(int status, const char* msg) {
    if (!p_client) return;
    reply_header.setValues(status, msg);
    reply_header.write(*p_client);
    endClient();
  }

  void replyChunked(const char* contentType, Stream& inputStream,
                    int status = 200, const char* msg = SUCCESS) override {
    if (!p_client) return;
    replyChunked(contentType, status, msg);
    HttpChunkWriter chunk_writer;
    uint8_t buffer[buffer_size];
    while (inputStream.available()) {
      int len = inputStream.readBytes(buffer, buffer_size);
      chunk_writer.writeChunk(*p_client, (const char*)buffer, len);
    }
    chunk_writer.writeEnd(*p_client);
    endClient();
  }

  void replyChunked(const char* contentType, int status = 200,
                    const char* msg = SUCCESS) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "replyChunked");
    reply_header.setValues(status, msg);
    reply_header.put(TRANSFER_ENCODING, CHUNKED);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, CON_KEEP_ALIVE);
    reply_header.write(*p_client);
  }

  void reply(const char* contentType, Stream& inputStream, int size,
             int status = 200, const char* msg = SUCCESS) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "stream");
    reply_header.setValues(status, msg);
    reply_header.put(CONTENT_LENGTH, size);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, CON_KEEP_ALIVE);
    reply_header.write(*p_client);
    uint8_t buffer[buffer_size];
    while (inputStream.available()) {
      int len = inputStream.readBytes(buffer, buffer_size);
      p_client->write((const uint8_t*)buffer, len);
    }
    endClient();
  }

  void reply(const char* contentType, size_t (*callback)(Print& out, void* ref),
             int status = 200, const char* msg = SUCCESS,
             void* ref = nullptr) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "via callback");
    NullPrint nop;
    size_t size = callback(nop, ref);
    reply_header.setValues(status, msg);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, CON_KEEP_ALIVE);
    reply_header.put(CONTENT_LENGTH, size);
    reply_header.write(*p_client);
    size_t size_eff = callback(*p_client, ref);
    if (size_eff != size) {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "HttpServer callback wrote %d bytes; expected %d",
                     size_eff, size);
    }
#if DLNA_LOG_XML
    callback(Serial, ref);
#endif

    #if DLNA_CHECK_XML_LENGTH
    StrPrint test;
    size_t test_len = callback(test, ref);
    if (strlen(test.c_str()) != size) {
      DlnaLogger.log(DlnaLogLevel::Error,
                     "HttpServer wrote %d bytes: expected %d / strlen: %d",
                     test_len, size, strlen(test.c_str()));
    }
#endif
    delay(200);
    endClient();
  }

  void reply(const char* contentType, const char* str, int status = 200,
             const char* msg = SUCCESS) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", str);
    int len = strlen(str);
    reply_header.setValues(status, msg);
    reply_header.put(CONTENT_LENGTH, len);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, CON_KEEP_ALIVE);
    reply_header.write(*p_client);
    p_client->write((const uint8_t*)str, len);
    endClient();
  }

  void reply(const char* contentType, const uint8_t* str, int len,
             int status = 200, const char* msg = SUCCESS) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s: %d bytes", contentType, len);
    reply_header.setValues(status, msg);
    reply_header.put(CONTENT_LENGTH, len);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, CON_KEEP_ALIVE);
    reply_header.write(*p_client);
    p_client->write((const uint8_t*)str, len);
    endClient();
  }

  void replyOK() override { reply("text/plain", "SUCCESS", 200, SUCCESS); }

  void replyNotFound() override {
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "404");
    reply("text/plain", "Page Not Found", 404, "Page Not Found");
  }

  void replyError(int err, const char* msg = "Internal Server Error") override {
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "error");
    reply("text/plain", msg, err, msg);
  }

  void endClient() override {
    if (p_client) {
      p_client->flush();
      p_client->stop();
    }
  }

  void crlf() override {
    if (p_client) p_client->println();
  }

  Client* client() override { return p_client; }

  HttpRequestHeader& requestHeader() { return request_header; }

  HttpReplyHeader& replyHeader() { return reply_header; }

  // Allows resizing the internal buffer
  void resize(size_t newSize) { buffer_size = newSize; }

 protected:
  ClientT* p_client = nullptr;
  HttpRequestHeader request_header;
  HttpReplyHeader reply_header;
  int buffer_size = 512;
};

}  // namespace tiny_dlna
