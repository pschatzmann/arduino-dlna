// Lightweight streaming parser for ConnectionManager GetProtocolInfo responses
#pragma once

#include <functional>
#include "basic/Str.h"
#include "http/Server/HttpRequest.h"
#include "dlna/Action.h"
// Client from Arduino core
#include <Client.h>

namespace tiny_dlna {


/**
 * @brief Streaming parser for ConnectionManager::GetProtocolInfo results
 *
 * This small utility parses the SOAP/XML reply of the GetProtocolInfo
 * action and extracts the comma-separated entries found inside the
 * <Source>...</Source> and <Sink>...</Sink> elements. It's designed for
 * low-memory environments and works in a streaming fashion without
 * buffering the full response. Two entry points are provided:
 * - parseFromClient(Client*, cb) for raw Client streams
 * - parseFromHttpRequest(HttpRequest*, cb) for chunk-aware HTTP replies
 *
 * The callback receives each protocol-info entry as a C-string and a
 * ProtocolRole indicating whether it came from Source or Sink.
 */
class XMLProtocolInfoParser {
 public:
  /**
   * Parse a streamed GetProtocolInfo XML result from a connected Client.
   * The parser searches for <Source>...</Source> and <Sink>...</Sink>
   * and invokes cb(entry, role) for each CSV entry found. This is
   * low-memory and does not buffer the full response.
   *
   * @param cli connected Client to read from
   * @param cb callback invoked for each parsed CSV entry
   * @return true if parsing completed or at least one entry was parsed,
   *         false on error (e.g., cli == nullptr)
   */
  static bool parseFromClient(
      Client* cli,
      std::function<void(const char* entry, ProtocolRole role)> cb) {
    if (!cli || !cb) return false;

    // Adapter reader for Client
    auto reader = [cli](uint8_t* buf, int maxLen) -> int {
      if (!cli->connected() || cli->available() == 0) return 0;
      return cli->read(buf, maxLen);
    };
    return parseWithReader(reader, cb);
  }

  /**
   * Parse a streamed GetProtocolInfo result using the chunk-aware
   * HttpRequest API (handles Transfer-Encoding: chunked internally).
   */
  static bool parseFromHttpRequest(
      tiny_dlna::HttpRequest* req,
      std::function<void(const char* entry, ProtocolRole role)> cb) {
    if (!req || !cb) return false;

    // Adapter reader for HttpRequest
    auto reader = [req](uint8_t* buf, int maxLen) -> int {
      if (!req->connected() || req->available() == 0) return 0;
      return req->read(buf, maxLen);
    };
    return parseWithReader(reader, cb);
  }

 protected:
  // Shared implementation used by both client-based and request-based
  // parsers. The reader lambda should return number of bytes read into
  // buf or 0 when no more data is available.
  static bool parseWithReader(
      std::function<int(uint8_t* buf, int maxLen)> reader,
      std::function<void(const char* entry, ProtocolRole role)> cb) {
    if (!reader || !cb) return false;

    enum CollectState { LOOKING, IN_SOURCE, IN_SINK } state = LOOKING;
    Str token(128);
    const char* endSource = "</Source>";
    const char* endSink = "</Sink>";
    int matchPos = 0;

    uint8_t buf[200];
    while (true) {
      int len = reader(buf, sizeof(buf));
      if (len <= 0) break;
      for (int i = 0; i < len; ++i) {
        char c = (char)buf[i];
        if (state == LOOKING) {
          static Str openBuf(16);
          openBuf.add(c);
          if (openBuf.length() > 64)
            openBuf =
                openBuf.substring(openBuf.length() - 64, openBuf.length());
          if (openBuf.indexOf("<Source") >= 0) {
            state = IN_SOURCE;
            token.clear();
            matchPos = 0;
            openBuf.clear();
            continue;
          }
          if (openBuf.indexOf("<Sink") >= 0) {
            state = IN_SINK;
            token.clear();
            matchPos = 0;
            openBuf.clear();
            continue;
          }
        } else if (state == IN_SOURCE) {
          if (c == endSource[matchPos]) {
            matchPos++;
            if (endSource[matchPos] == '\0') {
              Str t = token;
              t.trim();
              if (!t.isEmpty()) cb(t.c_str(), (ProtocolRole)0);
              token.clear();
              state = LOOKING;
              matchPos = 0;
              continue;
            }
            continue;
          } else {
            if (matchPos > 0) {
              for (int k = 0; k < matchPos; ++k) token.add(endSource[k]);
              matchPos = 0;
            }
            if (c == ',') {
              Str t = token;
              t.trim();
              if (!t.isEmpty()) cb(t.c_str(), (ProtocolRole)0);
              token.clear();
            } else {
              token.add(c);
            }
          }
        } else if (state == IN_SINK) {
          if (c == endSink[matchPos]) {
            matchPos++;
            if (endSink[matchPos] == '\0') {
              Str t = token;
              t.trim();
              if (!t.isEmpty()) cb(t.c_str(), (ProtocolRole)1);
              token.clear();
              state = LOOKING;
              matchPos = 0;
              continue;
            }
            continue;
          } else {
            if (matchPos > 0) {
              for (int k = 0; k < matchPos; ++k) token.add(endSink[k]);
              matchPos = 0;
            }
            if (c == ',') {
              Str t = token;
              t.trim();
              if (!t.isEmpty()) cb(t.c_str(), (ProtocolRole)1);
              token.clear();
            } else {
              token.add(c);
            }
          }
        }
      }
    }

    if (state == IN_SOURCE) {
      Str t = token;
      t.trim();
      if (!t.isEmpty()) cb(t.c_str(), (ProtocolRole)0);
    } else if (state == IN_SINK) {
      Str t = token;
      t.trim();
      if (!t.isEmpty()) cb(t.c_str(), (ProtocolRole)1);
    }

    return true;
  }
};

}  // namespace tiny_dlna
