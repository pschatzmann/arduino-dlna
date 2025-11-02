#pragma once
#include "Print.h"
#include "Str.h"
#include "dlna_config.h"

namespace tiny_dlna {

/**
 * @brief Print to a dynamic string
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StrPrint : public Print {
 public:
  StrPrint(int incSize = STR_PRINT_INC_SIZE) { inc_size = incSize; }
  size_t write(uint8_t ch) override {
    if (str.length() >= str.capacity() - 1) {
      str.setCapacity(str.length() + inc_size);
    }
    str.add((const char)ch);
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    size_t result = 0;
    for (int j = 0; j < size; j++) {
      result += write(buffer[j]);
    }
    // expand encoded entities if desired
    if (expand_encoded) {
      str.replaceAll("&amp;", "&");
      str.replaceAll("&lt;", "<");
      str.replaceAll("&gt;", ">");
    }

    return result;
  }

  const char* c_str() { return str.c_str(); }

  size_t length() { return str.length(); }

  void reset() { str.reset(); }

  void consume(int n) { str.remove(n); }

  void setExpandEncoded(bool flag) { expand_encoded = flag; }

  size_t printf(const char* fmt, ...) {
    char buf[MAX_PRINTF_SIZE];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) {
      write((const uint8_t*)buf, n);
      return n;
    }
    return 0;
  }

 protected:
  Str str{STR_PRINT_INITIAL_SIZE};
  int inc_size;
  bool expand_encoded = false;
};

}  // namespace tiny_dlna