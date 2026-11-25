#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace tiny_dlna {

/**
 * @brief Parser for a HTTP "Range" request header (RFC 7233)
 *
 * Supports a single byte range in the forms "bytes=first-last",
 * "bytes=first-" and "bytes=-suffixLength". Multiple ranges (separated by ",")
 * and other units are not supported: parse() returns false and the request
 * should be answered with the full content (200), which is permitted by the
 * RFC.
 *
 * Usage:
 * @code
 * HttpRange range;
 * if (range.parse(client.requestHeader().get("Range"), file.size())) {
 *   if (!range.isSatisfiable()) {
 *     client.replyRangeNotSatisfiable(file.size());
 *   } else {
 *     file.seek(range.start());
 *     client.replyRange(mime, file, file.size(), range.start(), range.end());
 *   }
 * } else {
 *   client.reply(mime, file, file.size());
 * }
 * @endcode
 */
class HttpRange {
 public:
  /// Parses the header value for content with the indicated total size.
  /// Returns true if a (single) byte range was requested: check
  /// isSatisfiable() to decide between 206 and 416. Returns false if there is
  /// no range or it can not be handled: reply with the full content.
  bool parse(const char* value, uint64_t totalSize) {
    is_valid = false;
    is_satisfiable = false;
    start_pos = 0;
    end_pos = 0;
    if (value == nullptr) return false;

    // skip leading spaces
    while (*value == ' ') value++;
    if (strncmp(value, "bytes=", 6) != 0) return false;
    const char* spec = value + 6;
    while (*spec == ' ') spec++;
    // multiple ranges are not supported
    if (strchr(spec, ',') != nullptr) return false;

    const char* dash = strchr(spec, '-');
    if (dash == nullptr) return false;

    bool has_first = dash != spec;
    bool has_last = isDigit(dash[1]);
    if (!has_first && !has_last) return false;
    if (!has_last) {
      // "first-": only spaces may follow the dash
      const char* p = dash + 1;
      while (*p == ' ') p++;
      if (*p != 0) return false;
    }

    uint64_t first = 0, last = 0;
    if (has_first && !parseNumber(spec, dash, first)) return false;
    if (has_last && !parseNumber(dash + 1, nullptr, last)) return false;

    is_valid = true;
    if (!has_first) {
      // suffix range: the last n bytes
      if (last == 0 || totalSize == 0) return true;  // not satisfiable
      start_pos = last >= totalSize ? 0 : totalSize - last;
      end_pos = totalSize - 1;
    } else {
      if (first >= totalSize) return true;  // not satisfiable
      if (has_last && last < first) {
        // syntactically invalid: ignore the range
        is_valid = false;
        return false;
      }
      start_pos = first;
      end_pos = (!has_last || last >= totalSize) ? totalSize - 1 : last;
    }
    is_satisfiable = true;
    return true;
  }

  /// true if the last parse() found a byte range which could be handled
  bool isValid() const { return is_valid; }
  /// true if the range overlaps with the content (else reply 416)
  bool isSatisfiable() const { return is_valid && is_satisfiable; }
  /// first byte position (inclusive)
  uint64_t start() const { return start_pos; }
  /// last byte position (inclusive)
  uint64_t end() const { return end_pos; }
  /// number of bytes in the range
  uint64_t length() const {
    return isSatisfiable() ? end_pos - start_pos + 1 : 0;
  }

 protected:
  bool is_valid = false;
  bool is_satisfiable = false;
  uint64_t start_pos = 0;
  uint64_t end_pos = 0;

  static bool isDigit(char c) { return c >= '0' && c <= '9'; }

  /// parses digits up to stop (or up to the first non digit if stop is null)
  static bool parseNumber(const char* str, const char* stop, uint64_t& result) {
    result = 0;
    const char* p = str;
    while ((stop == nullptr || p < stop) && isDigit(*p)) {
      result = result * 10 + (*p - '0');
      p++;
    }
    if (p == str) return false;
    // only trailing spaces are allowed
    if (stop != nullptr) return p == stop;
    while (*p == ' ') p++;
    return *p == 0;
  }
};

}  // namespace tiny_dlna
