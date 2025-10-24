#pragma once
#include "Print.h"
#include "Str.h"

namespace tiny_dlna {

/**
 * @brief Print to a dynamic string
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StrPrint : public Print {
 public:
  StrPrint(int incSize = 200) { inc_size = incSize; }
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
    return result;
  }

  const char* c_str() { return str.c_str(); }

  size_t length() { return str.length(); }

  void reset() { str.reset(); }

  void consume(int n) {
    str.remove(n);
  }

 protected:
  Str str{200};
  int inc_size;
};

}  // namespace tiny_dlna