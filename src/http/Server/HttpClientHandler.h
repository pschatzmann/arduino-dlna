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

  /// Determines if replies advertise "Connection: keep-alive" (default) or
  /// "Connection: close"
  void setKeepAlive(bool active) { is_keep_alive = active; }
  bool isKeepAlive() { return is_keep_alive; }

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
    if (isHeadRequest()) {
      endClient();
      return;
    }
    HttpChunkWriter chunk_writer;
    uint8_t buffer[buffer_size];
    bool ok = true;
    while (inputStream.available()) {
      int len = inputStream.readBytes(buffer, buffer_size);
      if (len <= 0) break;
      int written =
          chunk_writer.writeChunk(*p_client, (const char*)buffer, len);
      if (written != len) {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "replyChunked: client write failed (%d of %d bytes): "
                       "aborting",
                       written, len);
        ok = false;
        break;
      }
    }
    if (ok) chunk_writer.writeEnd(*p_client);
    endClient();
  }

  void replyChunked(const char* contentType, int status = 200,
                    const char* msg = SUCCESS) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "replyChunked");
    reply_header.setValues(status, msg);
    reply_header.put(TRANSFER_ENCODING, CHUNKED);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, is_keep_alive ? CON_KEEP_ALIVE : CON_CLOSE);
    reply_header.write(*p_client);
  }

  void reply(const char* contentType, Stream& inputStream, int size,
             int status = 200, const char* msg = SUCCESS) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "stream");
    reply_header.setValues(status, msg);
    reply_header.put(CONTENT_LENGTH, size);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, is_keep_alive ? CON_KEEP_ALIVE : CON_CLOSE);
    reply_header.write(*p_client);
    if (!isHeadRequest()) copyStream(inputStream, size);
    endClient();
  }

  /// Replies with 206 Partial Content for the byte range [start, end] of a
  /// content with totalSize bytes. The inputStream must already be positioned
  /// at start (e.g. file.seek(start)). See HttpRange for parsing the request.
  void replyRange(const char* contentType, Stream& inputStream,
                  uint64_t totalSize, uint64_t start, uint64_t end) override {
    if (!p_client) return;
    uint64_t len = end - start + 1;
    DlnaLogger.log(DlnaLogLevel::Info, "reply range %lu-%lu/%lu",
                   (unsigned long)start, (unsigned long)end,
                   (unsigned long)totalSize);
    char content_range[80];
    snprintf(content_range, sizeof(content_range), "bytes %llu-%llu/%llu",
             (unsigned long long)start, (unsigned long long)end,
             (unsigned long long)totalSize);
    reply_header.setValues(206, "Partial Content");
    reply_header.put(CONTENT_LENGTH, (int)len);
    reply_header.put(CONTENT_RANGE, content_range);
    reply_header.put(ACCEPT_RANGES, "bytes");
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, is_keep_alive ? CON_KEEP_ALIVE : CON_CLOSE);
    reply_header.write(*p_client);
    if (!isHeadRequest()) copyStream(inputStream, (int)len);
    endClient();
  }

  /// Replies with 416 Range Not Satisfiable for a content with totalSize bytes
  void replyRangeNotSatisfiable(uint64_t totalSize) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "416");
    char content_range[40];
    snprintf(content_range, sizeof(content_range), "bytes */%llu",
             (unsigned long long)totalSize);
    reply_header.setValues(416, "Range Not Satisfiable");
    reply_header.put(CONTENT_LENGTH, 0);
    reply_header.put(CONTENT_RANGE, content_range);
    reply_header.put(CONNECTION, is_keep_alive ? CON_KEEP_ALIVE : CON_CLOSE);
    reply_header.write(*p_client);
    endClient();
  }

  /// True if the current request is a HEAD request: replies send the
  /// header only
  bool isHeadRequest() { return request_header.method() == T_HEAD; }

  void reply(const char* contentType, size_t (*callback)(Print& out, void* ref),
             int status = 200, const char* msg = SUCCESS,
             void* ref = nullptr) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s", "via callback");
    NullPrint nop;
    size_t size = callback(nop, ref);
    reply_header.setValues(status, msg);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, is_keep_alive ? CON_KEEP_ALIVE : CON_CLOSE);
    reply_header.put(CONTENT_LENGTH, size);
    reply_header.write(*p_client);
    if (isHeadRequest()) {
      endClient();
      return;
    }
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
    reply_header.put(CONNECTION, is_keep_alive ? CON_KEEP_ALIVE : CON_CLOSE);
    reply_header.write(*p_client);
    if (!isHeadRequest()) p_client->write((const uint8_t*)str, len);
    endClient();
  }

  void reply(const char* contentType, const uint8_t* str, int len,
             int status = 200, const char* msg = SUCCESS) override {
    if (!p_client) return;
    DlnaLogger.log(DlnaLogLevel::Info, "reply %s: %d bytes", contentType, len);
    reply_header.setValues(status, msg);
    reply_header.put(CONTENT_LENGTH, len);
    reply_header.put(CONTENT_TYPE, contentType);
    reply_header.put(CONNECTION, is_keep_alive ? CON_KEEP_ALIVE : CON_CLOSE);
    reply_header.write(*p_client);
    if (!isHeadRequest()) p_client->write((const uint8_t*)str, len);
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
    if (!p_client) return;
    p_client->flush();
    p_client->stop();
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
  /// Copies up to size bytes (all if size <= 0) from the stream to the client;
  /// stops when the client write fails (e.g. the client closed the connection)
  void copyStream(Stream& inputStream, int size) {
    uint8_t buffer[buffer_size];
    int total = 0;
    while ((size <= 0 || total < size) && inputStream.available()) {
      int to_read = buffer_size;
      if (size > 0 && size - total < to_read) to_read = size - total;
      int len = inputStream.readBytes(buffer, to_read);
      if (len <= 0) break;
      size_t written = p_client->write((const uint8_t*)buffer, len);
      if (written != (size_t)len) {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "reply: client write failed after %d of %d bytes: "
                       "aborting",
                       total + (int)written, size);
        break;
      }
      total += len;
    }
  }

  ClientT* p_client = nullptr;
  HttpRequestHeader request_header;
  HttpReplyHeader reply_header;
  int buffer_size = 512;
  bool is_keep_alive = true;
};

}  // namespace tiny_dlna
