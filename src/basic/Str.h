#pragma once

#include "StrView.h"
#include "Vector.h"
#include <string>

namespace tiny_dlna {

/*
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
  Str() : s() {}

  explicit Str(size_t initialAllocatedLength) : s() {
    if (initialAllocatedLength > 0) s.reserve(initialAllocatedLength);
  }

  Str(int initialAllocatedLength) : s() {
    if (initialAllocatedLength > 0)
      s.reserve(static_cast<size_t>(initialAllocatedLength));
  }
  Str(const char* str) : s(str ? str : "") {}
  /* Construct from a C-string (null pointer treated as empty). */
  Str(const Str& other) = default;
  Str(Str&& other) noexcept = default;
  Str& operator=(const Str& other) = default;
  Str& operator=(Str&& other) noexcept = default;

  bool isEmpty() const { return s.empty(); }

  int length() const { return static_cast<int>(s.size()); }

  void copyFrom(const char* source, int len) {
    if (source == nullptr || len <= 0) {
      s.clear();
      return;
    }
    s.assign(source, static_cast<size_t>(len));
  }

  void substrView(StrView& from, int start, int end) {
    if (end <= start) {
      s.clear();
      return;
    }
    int l = end - start;
    s.assign(from.c_str() + start, static_cast<size_t>(l));
  }

  void substrView(const char* from, int start, int end) {
    if (from == nullptr || end <= start) {
      s.clear();
      return;
    }
    s.assign(from + start, static_cast<size_t>(end - start));
  }

  const char* c_str() const { return s.c_str(); }
  operator const char*() const { return c_str(); }


  void clear() { s.clear(); }

  // Append helpers (match StrView::add semantics)
  void add(const char* append) {
    if (append != nullptr) s.append(append);
  }
  void add(const uint8_t* append, int len) {
    if (append != nullptr && len > 0)
      s.append(reinterpret_cast<const char*>(append), static_cast<size_t>(len));
  }
  void add(char c) { s.push_back(c); }
  void add(int value) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d", value);
    if (n > 0) s.append(buf, static_cast<size_t>(n));
  }
  void add(double value, int precision = 2, int /*width*/ = 0) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%.*f", precision, value);
    if (n > 0) s.append(buf, static_cast<size_t>(n));
  }

  void set(const char* v) { s.assign(v ? v : ""); }
  void set(int v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", v);
    s.assign(buf);
  }
  void set(double v, int precision = 2, int width = 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", precision, v);
    (void)width;
    s.assign(buf);
  }

  Str& operator=(const char* v) {
    s.assign(v ? v : "");
    return *this;
  }
  Str& operator=(char* v) {
    s.assign(v ? v : "");
    return *this;
  }
  Str& operator=(int v) {
    set(v);
    return *this;
  }
  Str& operator=(double v) {
    set(v);
    return *this;
  }

  // reset / clearAll / release helpers
  void reset() { s.clear(); }
  void clearAll() { s.clear(); }
  void release() {
    s.clear();
    s.shrink_to_fit();
  }

  bool equals(const char* other) const {
    return (other != nullptr) && (s.compare(other) == 0);
  }
  bool contains(const char* sub) const {
    return (sub != nullptr) && (s.find(sub) != std::string::npos);
  }
  bool endsWith(const char* suffix) const {
    if (suffix == nullptr) return false;
    size_t sl = strlen(suffix);
    if (sl > s.size()) return false;
    return s.compare(s.size() - sl, sl, suffix) == 0;
  }

  bool equalsIgnoreCase(const char* other) const {
    if (other == nullptr) return false;
    if (s.size() != strlen(other)) return false;
    for (size_t i = 0; i < s.size(); ++i)
      if (tolower(s[i]) != tolower(static_cast<unsigned char>(other[i])))
        return false;
    return true;
  }

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

  bool startsWith(const char* prefix) const {
    if (prefix == nullptr) return false;
    size_t pl = strlen(prefix);
    if (pl > s.size()) return false;
    return s.compare(0, pl, prefix) == 0;
  }

  int indexOf(const char* substr, int start = 0) const {
    if (!substr) return -1;
    size_t pos = s.find(substr, static_cast<size_t>(start));
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }
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

  void trim() {
    rtrim();
    ltrim();
  }

  void ltrim() {
    size_t i = 0;
    while (i < s.size() && s.at(i) == ' ') ++i;
    if (i > 0) s.erase(0, i);
  }
  
  void rtrim() {
    while (!s.empty() && s.back() == ' ') s.pop_back();
  }

  void setLength(int newLen, bool addZero = true) {
    if (newLen < 0) return;
    if (newLen < static_cast<int>(s.size()))
      s.erase(static_cast<size_t>(newLen));
    else if (newLen > static_cast<int>(s.size()))
      s.append(static_cast<size_t>(newLen - s.size()), '\0');
  }

  void remove(const char* toRemove) {
    if (!toRemove) return;
    size_t pos = s.find(toRemove);
    if (pos != std::string::npos) s.erase(pos, strlen(toRemove));
  }

  bool replace(const char* toReplace, const char* replaced, int startPos = 0) {
    if (!toReplace || !replaced) return false;
    size_t pos = s.find(toReplace, static_cast<size_t>(startPos));
    if (pos == std::string::npos) return false;
    s.erase(pos, strlen(toReplace));
    s.insert(pos, replaced);
    return true;
  }

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

  Str substring(int start, int end) const {
    if (end <= start) return Str();
    size_t s1 = static_cast<size_t>(start);
    size_t len = static_cast<size_t>(end - start);
    return Str(s.substr(s1, len).c_str());
  }

  int toInt() const { return s.empty() ? 0 : atoi(s.c_str()); }
  long toLong() const { return s.empty() ? 0L : atol(s.c_str()); }
  double toDouble() const {
    return s.empty() ? 0.0 : strtod(s.c_str(), nullptr);
  }
  float toFloat() const { return static_cast<float>(toDouble()); }

  void toLowerCase() {
    for (size_t i = 0; i < s.size(); ++i)
      s[i] = static_cast<char>(tolower(static_cast<unsigned char>(s[i])));
  }
  void toUpperCase() {
    for (size_t i = 0; i < s.size(); ++i)
      s[i] = static_cast<char>(toupper(static_cast<unsigned char>(s[i])));
  }

  void setCapacity(size_t newLen) { s.reserve(newLen); }
  size_t capacity() const { return s.capacity(); }

  void operator+=(const char* v) { add(v); }
  void operator+=(int v) { add(v); }
  void operator+=(double v) { add(v); }
  void operator+=(const char c) { add(c); }

  bool operator==(const char* alt) const { return equals(alt); }
  bool operator!=(const char* alt) const { return !equals(alt); }

  char& operator[](size_t i) { return s[i]; }
  const char& operator[](size_t i) const { return s[i]; }

  bool isOnHeap() const { return true; }

 private:
  std::string s;
};

}  // namespace tiny_dlna
