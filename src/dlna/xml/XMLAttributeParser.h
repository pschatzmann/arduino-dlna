#pragma once

#include <cstring>
#include "basic/Str.h"

namespace tiny_dlna {

/**
 * @class XMLAttributeParser
 * @brief Small utility to extract attributes from a start-tag in an XML
 * fragment.
 *
 * This helper does not attempt full XML validation but is suitable for
 * small embedded parsing tasks (for example: extracting the
 * protocolInfo attribute from a DIDL-Lite <res ...> element).
 */
class XMLAttributeParser {
 public:
  /**
   * @brief Find the first occurrence of a start-tag and extract an attribute
   * value.
   *
   * This searches for the first occurrence of `tagName` (for example
   * "<res") in `xml` and looks for the attribute named `attrName` inside
   * that tag. If found, the attribute value (the text between the surrounding
   * double quotes) is copied into `outBuf` (null-terminated).
   *
   * @param xml Pointer to a null-terminated XML fragment to search.
   * @param tagName Start-tag to locate (e.g. "<res").
   * @param attrName Attribute name to extract (e.g. "protocolInfo" or
   *                 "protocolInfo="). The function expects a double-quoted
   *                 value (attrName="...").
   * @param outBuf Destination buffer where the extracted value will be
   *               written (null-terminated). Must be non-null.
   * @param bufSize Size of `outBuf` in bytes; if the attribute value is
   *                longer than `bufSize - 1` it will be truncated.
   * @return true if a non-empty attribute value was found and written into
   *         `outBuf`, false otherwise (invalid inputs, tag/attribute not
   *         found, malformed tag).
   */
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

  /**
   * @brief Extract the nth (1-based) colon-separated token from an
   * attribute value located on the first occurrence of `tagName`.
   *
   * This helper locates `tagName` (for example "<res") and extracts the
   * value of `attrName` (expects a double-quoted value). The attribute
   * value is split on ':' characters and the nth token (1-based) is copied
   * into `outBuf`.
   *
   * @param xml Pointer to a null-terminated XML fragment to search.
   * @param tagName Start-tag to locate (e.g. "<res"). Only the first
   *                occurrence is scanned.
   * @param attrName Attribute name to extract (with or without trailing
   *                 '='). The function expects a double-quoted value.
   * @param n 1-based index of the colon-separated token to return.
   * @param outBuf Destination buffer where the token will be written
   *               (null-terminated). Must be non-null.
   * @param bufSize Size of `outBuf` in bytes; if the token is longer than
   *                `bufSize - 1` it will be truncated.
   * @return true if the requested token was found and written into `outBuf`,
   *         false otherwise (invalid inputs, tag/attribute not found, index
   *         out of range).
   *
   * Example:
   *   attrName = "protocolInfo" (or "protocolInfo="), n = 3
   *   protocolInfo="http-get:*:audio/mpeg:*" -> returns "audio/mpeg".
   */
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
