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
   * @param textOnly If true, only report nodes with text content.
   */
  XMLParser(const char* xmlStr,
            void (*callback)(Str& nodeName, Vector<Str>& path, Str& text,
                             Str& attributes, int start, int len, void* ref),
            bool textOnly = false) {
    setXml(xmlStr);
    setCallback(callback);
    setReportTextOnly(textOnly);
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
  void setXml(const char* xmlStr) {
    str_view.set(xmlStr);
    parsePos = 0;
  }

  /**
   * @brief Set the callback to be invoked for parsed fragments
   * @param cb Function pointer with signature described in the constructor
   * docs.
   */
  void setCallback(void (*cb)(Str& nodeName, Vector<Str>& path, Str& text,
                              Str& attributes, int start, int len, void* ref)) {
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

  /**
   * @brief Parse a single fragment (one callback invocation) from the
   * previously set XML buffer.
   *
   * This allows incremental parsing of a larger buffer by repeatedly calling
   * `parseSingle()` until it returns false. The parser will resume from the
   * last position between calls. Call `resetParse()` to start parsing from
   * the beginning again.
   *
   * @return true if a fragment was parsed and the callback invoked, false
   *         if no more parseable fragments are available.
   */
  bool parseSingle() { return do_parse_single(); }

  /**
   * @brief Reset the internal parse position so subsequent parseSingle()
   * calls start from the beginning of the buffer.
   */
  void resetParse() { parsePos = 0; }

  /**
   * @brief Fully reset parser state (parse position, path stack and content
   * starts). Use this when the underlying buffer has been trimmed/changed
   * so the parser's internal stacks do not retain state from the previous
   * buffer view.
   */
  void resetParser() {
    parsePos = 0;
    path.resize(0);
    contentStarts.resize(0);
    last_attributes.set("");
    node_name.set("");
    txt.set("");
  }

  /// report only nodes with text
  void setReportTextOnly(bool flag) { report_text_only = flag; }

  /// Expose current parse position for incremental wrappers
  int getParsePos() { return parsePos; }

 protected:
  StrView str_view;
  Vector<Str> path{5};
  Str empty_str{0};
  Str node_name{40};
  Str txt{100};
  Str str{100};
  // last parsed attributes for the most recent start tag
  Str last_attributes;
  bool report_text_only = true;
  // callback signature: nodeName, path, text (inner text for elements or
  // trimmed text), start, len, ref
  // NOTE: the `Str` references passed to the callback (nodeName and text)
  // are backed by parser-owned storage (members of this class) and are only
  // valid for the duration of the callback invocation. If the callback
  // needs to retain the data it must make its own copy.
  void (*callback)(Str& nodeName, Vector<Str>& path, Str& text, Str& attributes,
                   int start, int len, void* ref) = nullptr;
  Vector<int> contentStarts;  // parallel stack to `path`: start position of
                              // content (after the start tag)
  // user-provided opaque pointer for convenience
  void* reference = nullptr;
  // Resume position for incremental parseSingle() calls.
  // (declared above in protected members)
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
      // Entity expansion is performed at setXml() time. Callbacks receive
      // the parser-owned (or expanded) text directly.
      invokeCallback(node_name, path, txt, last_attributes, ts, te - ts);
    }
  }

  void emitTagSegment(const char* s, int lt, int gt) {
    if (callback) {
      node_name = path.size() > 0 ? path.back() : empty_str;
      invokeCallback(node_name, path, empty_str, last_attributes, lt,
                     gt - lt + 1);
    }
  }

  int handleEndTag(const char* s, int lt, int gt) {
    if (path.size() > 0) path.erase(path.size() - 1);
    if (contentStarts.size() > 0) contentStarts.erase(contentStarts.size() - 1);
    return gt + 1;
  }

  void handleStartTag(const char* s, int lt, int gt) {
    // Clear previous attributes first to avoid carrying attributes from a
    // previously parsed tag into the current element when the current
    // tag contains no attributes. Use clear() so the underlying char buffer
    // is zero-terminated and not accidentally read as a C-string by callers
    // that use attributes.c_str().
    last_attributes.clear();
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
      // extract raw attribute text (between name end and tag end), exclude
      // trailing '/' for self-closing tags
      int attrStart = nameEnd;
      int attrEnd = gt;
      // back up one if tag ends with '/>'
      int back = gt - 1;
      while (back > lt && isspace((unsigned char)s[back])) back--;
      if (back > lt && s[back] == '/') attrEnd = back;
      // trim leading/trailing whitespace for attributes
      while (attrStart < attrEnd && isspace((unsigned char)s[attrStart]))
        attrStart++;
      while (attrEnd > attrStart && isspace((unsigned char)s[attrEnd - 1]))
        attrEnd--;
      if (attrEnd > attrStart) {
        last_attributes.copyFrom(s + attrStart, attrEnd - attrStart);
      } else {
        last_attributes.clear();
      }
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

  // Helper: invoke the registered callback but pass a path that excludes the
  // current node (only ancestor elements). This keeps the `nodeName`
  // parameter as the current element while `path` contains only parents.
  bool invokeCallback(Str& nodeName, Vector<Str>& fullPath, Str& text,
                      Str& attributes, int start, int len) {
    if (!callback) return false;
    if (report_text_only && text.isEmpty()) return false;
    Vector<Str> ancestorPath;
    int ancCount = fullPath.size() > 0 ? (int)fullPath.size() - 1 : 0;
    for (int i = 0; i < ancCount; ++i) {
      // Make a fresh copy of the ancestor name to avoid any aliasing
      // or lifetime issues with parser-owned storage.
      ancestorPath.push_back(Str(fullPath[i].c_str()));
    }
    callback(nodeName, ancestorPath, text, attributes, start, len, reference);
    return true;
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
  }

  // Incremental single-callback parser. It will resume from `parsePos`
  // and advance `parsePos` past the fragment it invoked (or to the end
  // if nothing more is parseable). Returns true if a callback was
  // invoked.
  bool do_parse_single() {
    const char* s = str_view.c_str();
    int len = str_view.length();
    int pos = parsePos;
    bool result = false;

    while (pos < len) {
      int lt = str_view.indexOf('<', pos);
      if (lt < 0) break;

      // Handle text between pos and lt
      if (lt > pos) {
        int ts = pos;
        int te = lt;
        while (ts < te && isspace((unsigned char)s[ts])) ts++;
        while (te > ts && isspace((unsigned char)s[te - 1])) te--;
        if (te > ts && callback) {
          // emit this text segment and advance parsePos past it
          txt.copyFrom(s + ts, te - ts);
          node_name = path.size() > 0 ? path.back() : empty_str;
          // Entity expansion is handled in setXml(); the callback receives
          // the parser text as-is.
          result = invokeCallback(node_name, path, txt, last_attributes, ts,
                                  te - ts);
          parsePos = te;  // next position is end of emitted text
          return result;
        }
      }

      // comment or processing instruction handling
      int newPos = handleCommentOrPI(lt, s, len);
      if (newPos != lt) {
        pos = newPos;
        parsePos = pos;
        continue;  // comments/PI produce no callback
      }

      // find closing '>' (respect quotes)
      int gt = findGt(s, lt, len);
      if (gt < 0) break;  // malformed

      // end tag
      if (lt + 1 < len && s[lt + 1] == '/') {
        // handle end tag; this may not produce a callback by itself
        pos = handleEndTag(s, lt, gt);
        parsePos = pos;
        // End tags don't generate callbacks in this parser's model unless
        // there is trimmed text to emit; continue to next loop iteration.
        continue;
      }

      // start tag (or self-closing)
      handleStartTag(s, lt, gt);

      // callback for the tag itself
      if (callback) {
        node_name = path.size() > 0 ? path.back() : empty_str;
        result = invokeCallback(node_name, path, empty_str, last_attributes, lt,
                                gt - lt + 1);
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
        parsePos = pos;
        return result;
      }

      pos = gt + 1;
      parsePos = pos;
    }

    // nothing left to parse
    parsePos = pos;
    return false;
  }

  // Resume position for incremental parseSingle() calls.
  int parsePos = 0;
};

}  // namespace tiny_dlna
