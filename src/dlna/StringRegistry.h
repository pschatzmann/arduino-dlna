#pragma once
#include "basic/Str.h"

namespace tiny_dlna {
/**
 * @brief Make sure that a string is stored only once
 *
 * Strings are stored as heap-allocated C strings (char*) so the returned
 * pointer remains valid even if the internal container reallocates. This
 * avoids dangling pointers caused by moving objects that own their buffers.
 */
class StringRegistry {
 public:
  /// adds a string (returns pointer to stable, owned C-string)
  const char* add(const char* in) {
    if (in == nullptr) return nullptr;
    // check existing strings
    for (int i = 0; i < strings.size(); ++i) {
      const char* s = strings[i];
      if (s != nullptr && strcmp(s, in) == 0) return s;
    }
    DlnaLogger.log(DlnaLogLevel::Info, "StringRegistry::add: %s", in);
    // allocate a stable copy
    size_t len = strlen(in);
    char* copy = new char[len + 1];
    memcpy(copy, in, len + 1);
    strings.push_back(copy);
    return strings[strings.size() - 1];
  }

  void clear() {
    // free owned C-strings
    for (int i = 0; i < strings.size(); ++i) {
      char* s = strings[i];
      if (s) delete[] s;
    }
    strings.clear();
  }

  /// Reports the number of strings
  size_t count() { return strings.size(); }

  /// Reports the total size of all allocated strings
  size_t size() {
    size_t total = 0;
    for (int i = 0; i < strings.size(); ++i) {
      const char* s = strings[i];
      if (s) total += strlen(s);
    }
    return total;
  }

  /// Return str if not null, alt otherwise
  static const char* nullStr(const char* str, const char* alt = "") {
    return str != nullptr ? str : alt;
  }

  /// Return str if not null, alt otherwise
  static const char* nullStr(Str& str, const char* empty = "(null)") {
    return str.isEmpty() ? empty : str.c_str();
  }
 

 protected:
  Vector<char*> strings;
};

}  // namespace tiny_dlna