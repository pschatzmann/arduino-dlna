#pragma once

#include "XMLParser.h"
#include "basic/StrPrint.h"

namespace tiny_dlna {

/**
 * @brief Helper class that implements a Print interface to accumulate XML data
 * and then parse it using XMLParser.
 *
 * This class allows you to write XML data using the Print interface, accumulate
 * it in an internal buffer, and then parse the buffer using an XMLParser. The
 * parsing result is returned via output parameters.
 */
class XMLParserPrint : public Print {
 public:
  /**
   * @brief Constructor
   */
  XMLParserPrint() {
    p.setCallback(&XMLParserPrint::wrapperCallback);
    p.setReference(&cbref);
    p.setReportTextOnly(false);
  }

  /**
   * @brief Writes a single byte to the buffer (Print interface)
   * @param ch Byte to write
   * @return Number of bytes written
   */
  size_t write(uint8_t ch) override { return buffer.write(ch); }

  /**
   * @brief Writes a block of data to the buffer (Print interface)
   * @param data Pointer to data
   * @param size Number of bytes to write
   * @return Number of bytes written
   */
  size_t write(const uint8_t* data, size_t size) override {
    return buffer.write(data, size);
  }

  /**
   * @brief Forwards expand-entities setting to the underlying XMLParser
   * @param flag If true, expand encoded entities
   */
  void setExpandEncoded(bool flag) { buffer.setExpandEncoded(flag); }

  /**
   * @brief Parses the accumulated XML data and returns results via output
   * parameters
   * @param outNodeName Output: node name
   * @param outPath Output: path vector
   * @param outText Output: text content
   * @param outAttributes Output: attributes string
   * @return True if parsing was successful, false otherwise
   */
  bool parse(Str& outNodeName, Vector<Str>& outPath, Str& outText,
             Str& outAttributes) {
    const char* data = buffer.c_str();
    size_t total_len = buffer.length();
    if (data == nullptr || total_len == 0) return false;

    cbref.outNode = &outNodeName;
    cbref.outPath = &outPath;
    cbref.outText = &outText;
    cbref.outAttrs = &outAttributes;
    // clear previous outputs so callers get an empty result on failure
    if (cbref.outNode) cbref.outNode->clear();
    if (cbref.outPath) cbref.outPath->resize(0);
    if (cbref.outText) cbref.outText->clear();
    if (cbref.outAttrs) cbref.outAttrs->clear();

    p.setXml(data);
    // mark as not-yet-found
    cbref.start = -1;
    cbref.len = 0;
    bool parsed = p.parseSingle();
    buffer.consume(p.getParsePos());
    return parsed;
  }

  /**
   * @brief Resets the internal buffer
   */
  void end() {
    p.end();
    buffer.reset();
    if (cbref.outNode) cbref.outNode->clear();
    if (cbref.outPath) cbref.outPath->resize(0);
    if (cbref.outText) cbref.outText->clear();
    if (cbref.outAttrs) cbref.outAttrs->clear();
  }

  /**
   * @brief Returns the internal buffer as a C string
   * @return Pointer to buffer data
   */
  const char* c_str() { return buffer.c_str(); }

  /**
   * @brief Returns the length of the internal buffer
   * @return Buffer length
   */
  size_t length() { return buffer.length(); }

 protected:
  /**
   * @brief Internal buffer for accumulating XML data
   */
  StrPrint buffer;

  /**
   * @brief XMLParser instance used for parsing
   */
  XMLParser p;

  /**
   * @brief Internal callback reference used to capture the callback data from
   * XMLParser and forward it to the caller.
   */
  struct CBRef {
    /** @brief Start position of parsed data */
    int start = 0;
    /** @brief Length of parsed data */
    int len = 0;
    /** @brief Output node name pointer */
    Str* outNode = nullptr;
    /** @brief Output path vector pointer */
    Vector<Str>* outPath = nullptr;
    /** @brief Output text pointer */
    Str* outText = nullptr;
    /** @brief Output attributes pointer */
    Str* outAttrs = nullptr;
  } cbref;

  /**
   * @brief Static wrapper used as XMLParser callback. Copies the first
   * invocation's data into the CBRef and forwards the event to the
   * user-provided callback if present.
   * @param nodeName Name of the node
   * @param path Path vector
   * @param text Text content
   * @param attributes Attributes string
   * @param start Start position
   * @param len Length
   * @param ref Reference to CBRef
   */
  static void wrapperCallback(Str& nodeName, Vector<Str>& path, Str& text,
                              Str& attributes, int start, int len, void* ref) {
    CBRef* r = static_cast<CBRef*>(ref);
    if (!r) return;
    r->start = start;
    r->len = len;
    if (r->outNode) *(r->outNode) = nodeName;
    if (r->outText) *(r->outText) = text;
    if (r->outAttrs) {
      if (attributes.isEmpty())
        r->outAttrs->reset();
      else
        *(r->outAttrs) = attributes;
    }
    if (r->outPath) {
      r->outPath->resize(0);
      for (int i = 0; i < path.size(); ++i) {
        r->outPath->push_back(path[i]);
      }
    }
  }
};

}  // namespace tiny_dlna
