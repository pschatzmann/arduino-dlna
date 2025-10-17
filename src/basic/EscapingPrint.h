#pragma once
#include "Print.h"

namespace tiny_dlna {

/// @brief Print wrapper that escapes & < > while forwarding to an underlying Print
struct EscapingPrint : public Print {
  Print& dest;
  EscapingPrint(Print& d) : dest(d) {}
  size_t write(uint8_t c) override {
    if (c == '&') return dest.print("&amp;");
    if (c == '<') return dest.print("&lt;");
    if (c == '>') return dest.print("&gt;");
    return dest.write(&c, 1);
  }
  size_t write(const uint8_t* buffer, size_t size) override {
    size_t r = 0;
    for (size_t i = 0; i < size; ++i) r += write(buffer[i]);
    return r;
  }
  int available() { return 0; }
};

} // namespace tiny_dlna
