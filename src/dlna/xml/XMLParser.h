#pragma once

#include "Print.h"
#include "assert.h"
#include "basic/Str.h"
#include "basic/StrView.h"
#include "basic/Vector.h"

namespace tiny_dlna {
/**
 * @brief Lightweight streaming XML parser
 *
 * XMLParser performs a single-pass, forgiving parse of a provided XML buffer
 * and calls a user-provided callback for discovered fragments. It is
 * intentionally small and suitable for embedded use-cases in this project
 * (SCPD fragments, DIDL snippets, etc.). It does not implement the full XML
 * specification (no DTDs, no entity decoding, minimal attribute handling).
 *
 * The callback receives both a small copied `Str` for convenience and the
 * numeric start/length offsets into the original buffer so callers can avoid
 * additional copies when desired.
 *
 * Threading / lifetime notes:
 * - The parser stores a non-owning `StrView` to the buffer provided via
 *   `setXml()`; that buffer must remain valid until `parse()` completes.
 */
class XMLParser {
 public:
  /** Default constructor. Use `setXml()` and `setCallback()` before parse(). */
  XMLParser() = default;
  /**
   * @brief Construct with XML buffer and callback
   * @param xmlStr Null-terminated XML buffer. The buffer is *not* copied; it
   *               must remain valid for the duration of parse().
   * @param callback Function pointer invoked for node events. Signature:
   *   void callback(const Str& nodeName, const Vector<Str>& path, const Str&
   * text, int start, int len, void* ref)
   * - nodeName: name of the current XML node (empty for no current node)
   * - path: current element path (top is the current element)
   * - text: element inner text for end-tag events (or trimmed text for text
   * nodes)
   * - start,len: numeric offset/length in the original buffer for the reported
   * fragment
   */
  XMLParser(const char* xmlStr,
            void (*callback)(Str& nodeName, Vector<Str>& path,
                             Str& text, int start, int len, void* ref)) {
    setXml(xmlStr);
    setCallback(callback);
  }

  /**
   * @brief Attach an opaque user pointer to the parser instance
   * @param ref Arbitrary user data. The parser does not own or manipulate
   *            this pointer; it is provided for callbacks or external use.
   */
  void setReference(void* ref) { reference = ref; }

  // Setters
  /**
   * @brief Set the XML buffer to parse
   * @param xmlStr Null-terminated XML buffer. The parser stores a non-owning
   *               view — the buffer must remain valid until parsing finishes.
   */
  void setXml(const char* xmlStr) { str_view = xmlStr; }

  /**
   * @brief Set the callback to be invoked for parsed fragments
   * @param cb Function pointer with signature described in the constructor
   * docs.
   */
  void setCallback(void (*cb)(Str& nodeName, Vector<Str>& path,
                              Str& text, int start, int len, void* ref)) {
    this->callback = cb;
  }

  /**
   * @brief Parse the previously set XML buffer and invoke the callback.
   *
   * The callback is invoked synchronously while `parse()` runs. After
   * `parse()` returns there are no outstanding callbacks and the caller may
   * reuse or free the XML buffer.
   */
  void parse() { do_parse(); }

 protected:
  StrView str_view;
  Vector<Str> path;
  Str empty_str;
  Str node_name;
  Str txt;
  Str str;

  // callback signature: nodeName, path, text (inner text for elements or
  // trimmed text), start, len, ref
  // NOTE: the `Str` references passed to the callback (nodeName and text)
  // are backed by parser-owned storage (members of this class) and are only
  // valid for the duration of the callback invocation. If the callback
  // needs to retain the data it must make its own copy.
  void (*callback)(Str& nodeName, Vector<Str>& path,
                   Str& text, int start, int len, void* ref) = nullptr;
  Vector<int> contentStarts;  // parallel stack to `path`: start position of
                              // content (after the start tag)
  // user-provided opaque pointer for convenience
  void* reference = nullptr;
  // Helper methods used by the parser loop (kept protected for
  // testing/extension)
  int findGt(const char* s, int start, int len) {
    int gt = -1;
    bool inQuote = false;
    char qchar = 0;
    for (int i = start + 1; i < len; ++i) {
      char c = s[i];
      if (!inQuote && (c == '"' || c == '\'')) {
        inQuote = true;
        qchar = c;
      } else if (inQuote && c == qchar) {
        inQuote = false;
      } else if (!inQuote && c == '>') {
        gt = i;
        break;
      }
    }
    return gt;
  }

  void emitTextSegment(const char* s, int ts, int te) {
    // trim whitespace bounds already provided by caller when appropriate
    if (te > ts && callback) {
      txt.copyFrom(s + ts, te - ts);
      node_name = path.size() > 0 ? path.back() : empty_str;
      callback(node_name, path, txt, ts, te - ts, reference);
    }
  }

  void emitTagSegment(const char* s, int lt, int gt) {
    if (callback) {
      node_name = path.size() > 0 ? path.back() : empty_str;
      callback(node_name, path, empty_str, lt, gt - lt + 1, reference);
    }
  }

  int handleEndTag(const char* s, int lt, int gt) {
    // compute inner content range and invoke callback before popping
    if (contentStarts.size() > 0 && callback) {
      int innerStart = contentStarts.back();
      int innerLen = lt - innerStart;
      if (innerLen > 0) {
        str.copyFrom(s + innerStart, innerLen);
        node_name = path.size() > 0 ? path.back() : empty_str;
        callback(node_name, path, str, innerStart, innerLen, reference);
      }
    }
    if (path.size() > 0) path.erase(path.size() - 1);
    if (contentStarts.size() > 0) contentStarts.erase(contentStarts.size() - 1);
    return gt + 1;
  }

  void handleStartTag(const char* s, int lt, int gt) {
    int nameStart = lt + 1;
    while (nameStart < gt && isspace((unsigned char)s[nameStart])) nameStart++;
    int nameEnd = nameStart;
    while (nameEnd < gt && !isspace((unsigned char)s[nameEnd]) &&
           s[nameEnd] != '/' && s[nameEnd] != '>')
      nameEnd++;
    if (nameEnd > nameStart) {
      node_name.copyFrom(s + nameStart, nameEnd - nameStart);
      path.push_back(node_name);
      int contentStart = gt + 1;
      contentStarts.push_back(contentStart);
    }
  }

  int handleCommentOrPI(int lt, const char* s, int len) {
    // comment <!-- ... -->
    if (lt + 4 < len && s[lt + 1] == '!' && s[lt + 2] == '-' &&
        s[lt + 3] == '-') {
      int end = str_view.indexOf("-->", lt + 4);
      return end < 0 ? len : end + 3;
    }
    // processing instruction <? ... ?>
    if (lt + 1 < len && s[lt + 1] == '?') {
      int end = str_view.indexOf("?>", lt + 2);
      return end < 0 ? len : end + 2;
    }
    return lt;  // not a comment/PI
  }

  void handleTrailingText(const char* s, int len) {
    int ts = 0;
    int te = len;
    while (ts < te && isspace((unsigned char)s[ts])) ts++;
    while (te > ts && isspace((unsigned char)s[te - 1])) te--;
    if (te > ts && callback) {
      str.copyFrom(s + ts, te - ts);
      node_name = path.size() > 0 ? path.back() : empty_str;
      callback(node_name, path, str, ts, te - ts, reference);
    }
  }
  // Run the parser. This is a small, forgiving parser suitable for the
  // embedded use-cases in this project (DIDL fragments, simple SCPD parsing).
  // It is not a full XML validator but handles start/end tags, comments,
  // processing instructions and self-closing tags. For each text node that
  // contains non-whitespace characters the provided callback is invoked with
  // the text and the current element path.
  void do_parse() {
    const char* s = str_view.c_str();
    int len = str_view.length();
    int pos = 0;

    while (pos < len) {
      int lt = str_view.indexOf('<', pos);
      if (lt < 0) break;

      // Handle text between pos and lt
      if (lt > pos) {
        int ts = pos;
        int te = lt;
        while (ts < te && isspace((unsigned char)s[ts])) ts++;
        while (te > ts && isspace((unsigned char)s[te - 1])) te--;
        emitTextSegment(s, ts, te);
      }

      // comment or processing instruction handling
      int newPos = handleCommentOrPI(lt, s, len);
      if (newPos != lt) {
        pos = newPos;
        continue;
      }

      // find closing '>' (respect quotes)
      int gt = findGt(s, lt, len);
      if (gt < 0) break;  // malformed

      // end tag
      if (lt + 1 < len && s[lt + 1] == '/') {
        pos = handleEndTag(s, lt, gt);
        continue;
      }

      // start tag (or self-closing)
      handleStartTag(s, lt, gt);

      // callback for the tag itself
      emitTagSegment(s, lt, gt);

      // detect self-closing and pop if needed
      bool selfClosing = false;
      int back = gt - 1;
      while (back > lt && isspace((unsigned char)s[back])) back--;
      if (back > lt && s[back] == '/') selfClosing = true;

      pos = gt + 1;

      if (selfClosing && path.size() > 0) {
        path.erase(path.size() - 1);
        if (contentStarts.size() > 0)
          contentStarts.erase(contentStarts.size() - 1);
      }
    }

    // trailing text
    int firstLt = str_view.indexOf('<', 0);
    if (firstLt < 0) firstLt = 0;
    if (firstLt < len) handleTrailingText(s, len);
  }
};

}  // namespace tiny_dlna
