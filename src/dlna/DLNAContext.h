#pragma once
#include "basic/Str.h"

namespace tiny_dlna {
/**
 * @brief DLNA context base class
 *
 * Some common global functions!
 */
class DLNAContext {
 public:
  /// Return str if not null, alt otherwise
  static const char* nullStr(const char* str, const char* alt = "") {
    return str != nullptr ? str : alt;
  }

  /// Return str if not null, alt otherwise
  static const char* nullStr(Str& str, const char* empty = "(null)") {
    return str.isEmpty() ? empty : str.c_str();
  }
 
};

}