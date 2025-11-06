#pragma once

#include "StrView.h"
#include "Vector.h"

namespace tiny_dlna {

#if DLNA_USE_STD

/*
 * @brief Heap-backed string utility used throughout tiny_dlna when
 * DLNA_USE_STD is enabled.
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

#else

/**
 * @brief String implementation which keeps the data on the heap. We grow the
 * allocated memory only if the copy source is not fitting.
 *
 * While it should be avoided to use a lot of heap allocatioins in
 * embedded devices it is sometimes more convinent to allocate a string
 * once on the heap and have the insurance that it might grow
 * if we need to process an unexpected size.
 *
 * We also need to use this if we want to manage a vecor of strings.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Str : public StrView {
 public:
  Str(int initialAllocatedLength = 0) {
    maxlen = initialAllocatedLength;
    is_const = false;
    grow(maxlen);
  }

  Str(const char* str) : Str() {
    if (str != nullptr) {
      len = strlen(str);
      maxlen = len;
      grow(maxlen);
      if (chars != nullptr) {
        strcpy(chars, str);
      }
    }
  }

  /// Convert StrView to Str
  Str(StrView& source) : Str() { set(source); }

  /// Copy constructor
  Str(const Str& source) : Str() { set(source); }

  /// Move constructor
  Str(Str&& obj) { move(obj); }

  /// Destructor
  ~Str() {
    // ensure underlying buffer is freed
    release();
  }

  /// Move assignment
  Str& operator=(Str&& obj) { return move(obj); }

  /// Copy assingment
  Str& operator=(const Str& obj) {
    // assert(&obj!=nullptr);
    set(obj.c_str());
    return *this;
  };

  bool isOnHeap() override { return true; }

  bool isConst() override { return false; }

  void operator=(const char* str) override { set(str); }

  void operator=(char* str) override { set(str); }

  void operator=(int v) override { set(v); }

  void operator=(double v) override { set(v); }

  size_t capacity() { return maxlen; }

  void setCapacity(size_t newLen) { grow(newLen); }

  // make sure that the max size is allocated
  void allocate(int len = -1) {
    int new_size = len < 0 ? maxlen : len;
    grow(new_size);
    this->len = new_size;
  }

  /// assigns a memory buffer
  void copyFrom(const char* source, int len, int maxlen = 0) {
    this->maxlen = maxlen == 0 ? len : maxlen;
    resize(this->maxlen);
    if (this->chars != nullptr) {
      this->len = len;
      this->is_const = false;
      memmove(this->chars, source, len);
      this->chars[len] = 0;
    }
  }

  /// Fills the string with len chars
  void setChars(char c, int len) {
    grow(this->maxlen);
    if (this->chars != nullptr) {
      for (int j = 0; j < len; j++) {
        this->chars[j] = c;
      }
      this->len = len;
      this->is_const = false;
      this->chars[len] = 0;
    }
  }

  /// url encode the string
  void urlEncode() {
    char temp[4];
    int new_size = 0;
    // Calculate the new size
    for (size_t i = 0; i < len; i++) {
      urlEncodeChar(chars[i], temp, 4);
      new_size += strlen(temp);
    }
    // build new string
    char result[new_size + 1];
    memset(result, 0, new_size + 1);
    for (size_t i = 0; i < len; i++) {
      urlEncodeChar(chars[i], temp, 4);
      strcat(result, temp);
    }
    // save result
    grow(new_size);
    strcpy(chars, result);
    // set the length to the actual encoded length
    this->len = strlen(chars);
  }

  /// decodes a url encoded string
  void urlDecode() {
    char szTemp[2];
    size_t i = 0;
    size_t result_idx = 0;
    while (i < len) {
      if (chars[i] == '%') {
        szTemp[0] = chars[i + 1];
        szTemp[1] = chars[i + 2];
        chars[result_idx] = strToBin(szTemp);
        i = i + 3;
      } else if (chars[i] == '+') {
        chars[result_idx] = ' ';
        i++;
      } else {
        chars[result_idx] += chars[i];
        i++;
      }
      result_idx++;
    }
    chars[result_idx] = 0;
    this->len = result_idx;
  }

  // just sets the len to 0
  void reset() {
    memset(vector.data(), 0, len);
    len = 0;
  }

  void clear() override {
    len = 0;
    maxlen = 0;
    // release the underlying buffer
    vector.reset();
    chars = nullptr;
  }

  void resize(int size) {
    vector.resize(size + 1);
    maxlen = size;
    len = size;
    chars = vector.data();
  }

  void swap(Str& other) {
    int tmp_len = len;
    int tmp_maxlen = maxlen;
    len = other.len;
    maxlen = other.maxlen;
    vector.swap(other.vector);
    chars = vector.data();
    other.chars = other.vector.data();
  }

  const char* c_str() const {
    const char* result = vector.data();
    return result == nullptr ? "" : result;
  }

  /// Release the internal heap buffer .
  void release() {
    // free the vector buffer (Vector::reset() frees the allocated array)
    vector.reset();
    // reset Str state
    chars = nullptr;
    len = 0;
    maxlen = 0;
    is_const = false;
  }
  
  /// copies a substring into the current string
  Str substring(int start, int end) {
    Str result;
    if (end > start && this->chars != nullptr) {
      int len = end - start;
      result.grow(len);
      len = len < this->maxlen ? len : this->maxlen;
      char* target = (char*)result.c_str();
      strncpy(target, this->c_str() + start, len);
      target[len] = 0;
    }
    return result;
  }

  /// removes the first n characters
  void remove(int n) {
    if (n >= this->length()) {
      reset();
    } else if (n <= 0) {
      return;
    } else {
      int remaining = this->length() - n;
      memmove(this->chars, this->chars + n, remaining);
      this->chars[remaining] = 0;
      this->len = remaining;
    }
  }

 protected:
  Vector<char> vector{0};

  Str& move(Str& other) {
    swap(other);
    other.clear();
    return *this;
  }

  bool grow(int newMaxLen) override {
    bool grown = false;
    // assert(newMaxLen < 1024 * 10);
    if (newMaxLen < 0) return false;
    if (newMaxLen == 0 && chars == nullptr) return false;

    if (chars == nullptr || newMaxLen > maxlen) {
      grown = true;
      // we use at minimum the defined maxlen
      int newSize = newMaxLen > maxlen ? newMaxLen : maxlen;
      vector.resize(newSize + 1);
      chars = &vector[0];
      maxlen = newSize;
    }
    return grown;
  }

  void urlEncodeChar(char c, char* result, int maxLen) {
    if (isalnum(c)) {
      snprintf(result, maxLen, "%c", c);
    } else if (isspace(c)) {
      snprintf(result, maxLen, "%s", "+");
    } else {
      snprintf(result, maxLen, "%%%X%X", c >> 4, c % 16);
    }
  }

  char charToInt(char ch) {
    if (ch >= '0' && ch <= '9') {
      return (char)(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
      return (char)(ch - 'a' + 10);
    }
    if (ch >= 'A' && ch <= 'F') {
      return (char)(ch - 'A' + 10);
    }
    return -1;
  }

  char strToBin(char* pString) {
    char szBuffer[2];
    char ch;
    szBuffer[0] = charToInt(pString[0]);    // make the B to 11 -- 00001011
    szBuffer[1] = charToInt(pString[1]);    // make the 0 to 0 -- 00000000
    ch = (szBuffer[0] << 4) | szBuffer[1];  // to change the BO to 10110000
    return ch;
  }
};

#endif

}  // namespace tiny_dlna
