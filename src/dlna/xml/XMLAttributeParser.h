#pragma once

#include <cstring>
#include "basic/Str.h"

namespace tiny_dlna {

// Small utility to extract attributes from a start-tag in an XML fragment.
// It does not attempt full XML validation but is safe for small embedded
// parsing tasks (e.g. DIDL <res ... protocolInfo="...">).
class XMLAttributeParser {
 public:
  // Find the first occurrence of `tagName` (e.g. "<res") in `xml` and
  // extract the value of attribute `attrName` into `outBuf` (size `bufSize`).
  // Returns true on success and fills outBuf (null-terminated). If the
  // attribute value is longer than bufSize-1, it will be truncated.
  static bool extractAttribute(const char* xml, const char* tagName,
                               const char* attrName, char* outBuf,
                               size_t bufSize) {
    if (!xml || !tagName || !attrName || !outBuf || bufSize == 0) return false;
    const char* p = strstr(xml, tagName);
    while (p) {
      const char* tagEnd = strchr(p, '>');
      if (!tagEnd) return false;
      const char* attrPos = strstr(p, attrName);
      if (attrPos && attrPos < tagEnd) {
        const char* q = strchr(attrPos, '"');
        if (!q || q > tagEnd) return false;
        q++;
        const char* qend = strchr(q, '"');
        if (!qend || qend > tagEnd) return false;
        size_t vlen = (size_t)(qend - q);
        size_t copyLen = vlen < (bufSize - 1) ? vlen : (bufSize - 1);
        if (copyLen > 0) memcpy(outBuf, q, copyLen);
        outBuf[copyLen] = '\0';
        return true;
      }
      p = strstr(p + 1, tagName);
    }
    return false;
  }

  // Generic helper: extract the nth (1-based) colon-separated token from the
  // value of attribute `attrName` on the first occurrence of `tagName`.
  //
  // Behavior and parameters:
  // - `xml`       : pointer to a null-terminated XML fragment to search.
  // - `tagName`   : start-tag to locate (for example "<res"). The first
  //                 occurrence is scanned; the search does not validate full
  //                 XML correctness.
  // - `attrName`  : attribute name to extract. Can be passed either with or
  //                 without the trailing '=' (e.g. "protocolInfo" or
  //                 "protocolInfo="). The function looks for the attribute
  //                 inside the located start-tag and expects a double-quoted
  //                 value: protocolInfo="...".
  // - `n`         : 1-based index of the colon-separated token to return.
  //                 n==1 returns the first token. Tokens are split on ':'
  //                 characters and are not trimmed; embedded XML entities are
  //                 not decoded.
  // - `outBuf`/`bufSize`: destination buffer and its size (must be > 0).
  //
  // Return value and truncation:
  // - On success the requested token (non-empty) is copied into `outBuf`,
  //   null-terminated. If the token is longer than `bufSize - 1` it will be
  //   truncated to fit and still null-terminated. The function returns true
  //   when a non-empty token was extracted into `outBuf`.
  // - The function returns false if any input is invalid, the tag or
  //   attribute cannot be found inside the tag, or the requested token index
  //   does not exist (or is empty).
  //
  // Example:
  //   attrName = "protocolInfo" (or "protocolInfo="), n = 3
  //   protocolInfo="http-get:*:audio/mpeg:*" -> returns "audio/mpeg".
  static bool extractAttributeToken(const char* xml, const char* tagName,
                                     const char* attrName, int n,
                                     char* outBuf, size_t bufSize) {
    if (!xml || !tagName || !attrName || n <= 0 || !outBuf || bufSize == 0) return false;
    char proto[256] = {0};
    bool ok = extractAttribute(xml, tagName, attrName, proto, sizeof(proto));
    if (!ok || proto[0] == '\0') return false;
    // proto format: token1:token2:token3:token4
    int col = 1;
    const char* tokenStart = proto;
    if (n > 1) {
      for (size_t i = 0; proto[i] != '\0'; ++i) {
        if (proto[i] == ':') {
          ++col;
          if (col == n) {
            tokenStart = proto + i + 1;
            break;
          }
        }
      }
    }
    if (!tokenStart || *tokenStart == '\0') return false;
    // copy token into outBuf until ':' or end
    size_t j = 0;
    for (; tokenStart[j] != '\0' && tokenStart[j] != ':' && j + 1 < bufSize; ++j) outBuf[j] = tokenStart[j];
    outBuf[j] = '\0';
    return outBuf[0] != '\0';
  }
};

} // namespace tiny_dlna
