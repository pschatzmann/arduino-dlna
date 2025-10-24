#pragma once

#include "XMLParser.h"
#include "basic/StrPrint.h"

namespace tiny_dlna {

/**
 * @brief Helper that implements a Print interface to accumulate XML data and
 * then parse it using XMLParser.
 */
class XMLParserPrint : public Print {
 public:
  XMLParserPrint(int reserve = 80) : buffer(reserve) {
    p.setCallback(&XMLParserPrint::wrapperCallback);
    p.setReference(&cbref);
    p.setReportTextOnly(false);
  }

  // Print interface
  size_t write(uint8_t ch) override { return buffer.write(ch); }

  size_t write(const uint8_t* data, size_t size) override {
    return buffer.write(data, size);
  }

  /// Forward expand-entities setting to the underlying XMLParser
  void setExpandEncoded(bool flag) { buffer.setExpandEncoded(flag); }

  bool parse(Str& outNodeName, Vector<Str>& outPath, Str& outText,
             Str& outAttributes) {
    const char* data = buffer.c_str();
    size_t total_len = buffer.length();
    if (data == nullptr || total_len == 0) return false;

    cbref.outNode = &outNodeName;
    cbref.outPath = &outPath;
    cbref.outText = &outText;
    cbref.outAttrs = &outAttributes;

    p.setXml(data);
    // clear any previous cbref values
    cbref.start = 0;
    cbref.len = 0;
    bool parsed = p.parseSingle();
    buffer.consume(p.getParsePos());
    return parsed;
  }

  void reset() { buffer.reset(); }

  const char* c_str() { return buffer.c_str(); }

  size_t length() { return buffer.length(); }

 protected:
  StrPrint buffer;
  XMLParser p;
  // Internal callback reference used to capture the callback data
  // from XMLParser and forward it to the caller.
  struct CBRef {
    int start = 0;
    int len = 0;
    Str* outNode = nullptr;
    Vector<Str>* outPath = nullptr;
    Str* outText = nullptr;
    Str* outAttrs = nullptr;
  } cbref;

  // Static wrapper used as XMLParser callback. It copies the first
  // invocation's data into the CBRef and forwards the event to the
  // user-provided callback if present.
  static void wrapperCallback(Str& nodeName, Vector<Str>& path, Str& text,
                              Str& attributes, int start, int len, void* ref) {
    CBRef* r = static_cast<CBRef*>(ref);
    if (!r) return;
    r->start = start;
    r->len = len;
    if (r->outNode) *(r->outNode) = nodeName.c_str();
    if (r->outText) *(r->outText) = text.c_str();
    if (r->outAttrs) {
      if (attributes.isEmpty())
        r->outAttrs->reset();
      else
        *(r->outAttrs) = attributes.c_str();
    }
    if (r->outPath) {
      r->outPath->resize(0);
      for (int i = 0; i < path.size(); ++i) {
        r->outPath->push_back(Str(path[i].c_str()));
      }
    }
  }
};

}  // namespace tiny_dlna
