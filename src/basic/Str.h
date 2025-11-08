#pragma once

#include "StrView.h"
#include "Vector.h"
#include <string>

namespace tiny_dlna {

/** 
 * @brief Heap-backed string utility used throughout tiny_dlna 
 *
 * This class intentionally uses composition around `std::string` and
 * forwards a small, controlled subset of operations used by the codebase.
 * It attempts to preserve the historical `Str` API (length as `int`,
 * `add`/`set` helpers, substring helpers, simple trimming and search
 * utilities) while keeping ownership explicit and avoiding exposure of
 * the entire `std::string` interface.
 *
 * Usage notes:
 * - Most callers treat `Str` similarly to the original project string
 *   type; it provides an implicit conversion to `const char*` for
 *   convenience when interoperating with legacy `StrView` APIs.
 * - The class is intended for embedded-friendly usage where a heap
 *   buffer is acceptable; `isOnHeap()` returns true for the callers that
 *   check allocation semantics.
 */
class Str {
 public:
  /// Construct empty string
  Str() : s() {}

  /// Construct reserving an initial capacity
  explicit Str(size_t initialAllocatedLength) : s() {
    if (initialAllocatedLength > 0) s.reserve(initialAllocatedLength);
  }

  /// Construct reserving an initial capacity (int overload)
  Str(int initialAllocatedLength) : s() {
    if (initialAllocatedLength > 0)
      s.reserve(static_cast<size_t>(initialAllocatedLength));
  }
  /// Construct from C-string (nullptr -> empty)
  Str(const char* str) : s(str ? str : "") {}
  /// Copy-construct
  Str(const Str& other) = default;
  /// Move-construct
  Str(Str&& other) noexcept = default;
  /// Copy-assign
  Str& operator=(const Str& other) = default;
  /// Move-assign
  Str& operator=(Str&& other) noexcept = default;

  /// True if empty
  bool isEmpty() const { return s.empty(); }

  /// Current length (int)
  int length() const { return static_cast<int>(s.size()); }

  /// Copy from raw buffer with length
  void copyFrom(const char* source, int len) {
    if (source == nullptr || len <= 0) {
      s.clear();
      return;
    }
    s.assign(source, static_cast<size_t>(len));
  }

  /// Assign substring view from StrView [start,end)
  void substrView(StrView& from, int start, int end) {
    if (end <= start) {
      s.clear();
      return;
    }
    int l = end - start;
    s.assign(from.c_str() + start, static_cast<size_t>(l));
  }

  /// Assign substring view from const char* [start,end)
  void substrView(const char* from, int start, int end) {
    if (from == nullptr || end <= start) {
      s.clear();
      return;
    }
    s.assign(from + start, static_cast<size_t>(end - start));
  }

  /// C-string pointer to internal buffer
  const char* c_str() const { return s.c_str(); }
  /// Implicit conversion to const char*
  operator const char*() const { return c_str(); }

  /// Clear contents (size -> 0)
  void clear() { s.clear(); }

  /// Append C-string (ignored if nullptr)
  void add(const char* append) {
    if (append != nullptr) s.append(append);
  }
  /// Append raw bytes
  void add(const uint8_t* append, int len) {
    if (append != nullptr && len > 0)
      s.append(reinterpret_cast<const char*>(append), static_cast<size_t>(len));
  }
  /// Append single character
  void add(char c) { s.push_back(c); }
  /// Append integer as decimal text
  void add(int value) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d", value);
    if (n > 0) s.append(buf, static_cast<size_t>(n));
  }
  /// Append floating point as text with precision
  void add(double value, int precision = 2, int /*width*/ = 0) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%.*f", precision, value);
    if (n > 0) s.append(buf, static_cast<size_t>(n));
  }

  /// Assign from C-string (nullptr -> empty)
  void set(const char* v) { s.assign(v ? v : ""); }
  /// Assign from integer
  void set(int v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", v);
    s.assign(buf);
  }
  /// Assign from floating point with precision
  void set(double v, int precision = 2, int width = 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", precision, v);
    (void)width;
    s.assign(buf);
  }

  /// Assign from C-string (operator=)
  Str& operator=(const char* v) {
    s.assign(v ? v : "");
    return *this;
  }
  /// Assign from char* (operator=)
  Str& operator=(char* v) {
    s.assign(v ? v : "");
    return *this;
  }
  /// Assign from integer (operator=)
  Str& operator=(int v) {
    set(v);
    return *this;
  }
  /// Assign from double (operator=)
  Str& operator=(double v) {
    set(v);
    return *this;
  }

  /// Clear contents (alias of clear)
  void reset() { s.clear(); }
  /// Clear contents (legacy alias)
  void clearAll() { s.clear(); }
  /// Clear and shrink capacity to fit
  void release() {
    s.clear();
    s.shrink_to_fit();
  }

  /// Exact string equality with C-string
  bool equals(const char* other) const {
    return (other != nullptr) && (s.compare(other) == 0);
  }
  /// True if substring occurs
  bool contains(const char* sub) const {
    return (sub != nullptr) && (s.find(sub) != std::string::npos);
  }
  /// True if ends with suffix (case-sensitive)
  bool endsWith(const char* suffix) const {
    if (suffix == nullptr) return false;
    size_t sl = strlen(suffix);
    if (sl > s.size()) return false;
    return s.compare(s.size() - sl, sl, suffix) == 0;
  }

  /// Case-insensitive equality with C-string
  bool equalsIgnoreCase(const char* other) const {
    if (other == nullptr) return false;
    if (s.size() != strlen(other)) return false;
    for (size_t i = 0; i < s.size(); ++i)
      if (tolower(s[i]) != tolower(static_cast<unsigned char>(other[i])))
        return false;
    return true;
  }

  /// True if ends with suffix (case-insensitive)
  bool endsWithIgnoreCase(const char* suffix) const {
    if (suffix == nullptr) return false;
    size_t sl = strlen(suffix);
    if (sl > s.size()) return false;
    size_t start = s.size() - sl;
    for (size_t i = 0; i < sl; ++i)
      if (tolower(s[start + i]) !=
          tolower(static_cast<unsigned char>(suffix[i])))
        return false;
    return true;
  }

  /// True if starts with prefix (case-sensitive)
  bool startsWith(const char* prefix) const {
    if (prefix == nullptr) return false;
    size_t pl = strlen(prefix);
    if (pl > s.size()) return false;
    return s.compare(0, pl, prefix) == 0;
  }

  /// Index of substring from position (or -1)
  int indexOf(const char* substr, int start = 0) const {
    if (!substr) return -1;
    size_t pos = s.find(substr, static_cast<size_t>(start));
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
  /// Index of character from position (or -1)
  int indexOf(char c, int start = 0) const {
    size_t pos = s.find(c, static_cast<size_t>(start));
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

  /// removes the first n characters
  void remove(int n) {
    if (n <= 0) return;
    if (n >= static_cast<int>(s.size()))
      s.clear();
    else
      s.erase(0, static_cast<size_t>(n));
  }

  /// Trim spaces on both ends
  void trim() {
    rtrim();
    ltrim();
  }

  /// Trim leading spaces
  void ltrim() {
    size_t i = 0;
    while (i < s.size() && s.at(i) == ' ') ++i;
    if (i > 0) s.erase(0, i);
  }
  
  /// Trim trailing spaces
  void rtrim() {
    while (!s.empty() && s.back() == ' ') s.pop_back();
  }

  /// Resize logical length (pads with NUL if growing)
  void setLength(int newLen, bool addZero = true) {
    if (newLen < 0) return;
    if (newLen < static_cast<int>(s.size()))
      s.erase(static_cast<size_t>(newLen));
    else if (newLen > static_cast<int>(s.size()))
      s.append(static_cast<size_t>(newLen - s.size()), '\0');
  }

  /// Remove first occurrence of substring (no-op if not found)
  void remove(const char* toRemove) {
    if (!toRemove) return;
    size_t pos = s.find(toRemove);
    if (pos != std::string::npos) s.erase(pos, strlen(toRemove));
  }

  /// Replace first occurrence of toReplace with replaced starting at startPos
  bool replace(const char* toReplace, const char* replaced, int startPos = 0) {
    if (!toReplace || !replaced) return false;
    size_t pos = s.find(toReplace, static_cast<size_t>(startPos));
    if (pos == std::string::npos) return false;
    s.erase(pos, strlen(toReplace));
    s.insert(pos, replaced);
    return true;
  }

  /// Replace all occurrences of toReplace with replaced; returns count
  int replaceAll(const char* toReplace, const char* replaced) {
    if (!toReplace || !replaced) return 0;
    int found = 0;
    size_t pos = 0;
    while ((pos = s.find(toReplace, pos)) != std::string::npos) {
      s.erase(pos, strlen(toReplace));
      s.insert(pos, replaced);
      pos += strlen(replaced);
      ++found;
    }
    return found;
  }

  /// Return substring [start,end) as a new Str
  Str substring(int start, int end) const {
    if (end <= start) return Str();
    size_t s1 = static_cast<size_t>(start);
    size_t len = static_cast<size_t>(end - start);
    return Str(s.substr(s1, len).c_str());
  }

  /// Convert to int (0 if empty)
  int toInt() const { return s.empty() ? 0 : atoi(s.c_str()); }
  /// Convert to long (0 if empty)
  long toLong() const { return s.empty() ? 0L : atol(s.c_str()); }
  /// Convert to double (0.0 if empty)
  double toDouble() const {
    return s.empty() ? 0.0 : strtod(s.c_str(), nullptr);
  }
  /// Convert to float (0.0f if empty)
  float toFloat() const { return static_cast<float>(toDouble()); }

  /// Lowercase in-place
  void toLowerCase() {
    for (size_t i = 0; i < s.size(); ++i)
      s[i] = static_cast<char>(tolower(static_cast<unsigned char>(s[i])));
  }
  /// Uppercase in-place
  void toUpperCase() {
    for (size_t i = 0; i < s.size(); ++i)
      s[i] = static_cast<char>(toupper(static_cast<unsigned char>(s[i])));
  }

  /// Reserve capacity
  void setCapacity(size_t newLen) { s.reserve(newLen); }
  /// Current capacity
  size_t capacity() const { return s.capacity(); }

  /// Append via operator+= (C-string)
  void operator+=(const char* v) { add(v); }
  /// Append via operator+= (int)
  void operator+=(int v) { add(v); }
  /// Append via operator+= (double)
  void operator+=(double v) { add(v); }
  /// Append via operator+= (char)
  void operator+=(const char c) { add(c); }

  /// Equality with C-string
  bool operator==(const char* alt) const { return equals(alt); }
  /// Inequality with C-string
  bool operator!=(const char* alt) const { return !equals(alt); }

  /// Element access (mutable)
  char& operator[](size_t i) { return s[i]; }
  /// Element access (const)
  const char& operator[](size_t i) const { return s[i]; }

  /// Always true: storage is heap-backed
  bool isOnHeap() const { return true; }

 private:
  std::string s;
};

}  // namespace tiny_dlna
