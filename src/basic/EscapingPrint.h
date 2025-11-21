#pragma once
#include "Print.h"

namespace tiny_dlna {

/// @brief Print wrapper that escapes & < > " ' while forwarding to an
/// underlying Print. Returns the expanded output length (not input bytes
/// consumed).
struct EscapingPrint : public Print {
  EscapingPrint(Print& d) : dest(d) {}

  size_t write(uint8_t c) override {
    // Always return the logical expanded length, regardless of what
    // dest.write() returns
    switch (c) {
      case '&':
        if (dest.print("&amp;") != 5) {
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "EscapingPrint: failed to write &amp;");
        }
        return 5;
      case '<':
        if(dest.print("&lt;") != 4){
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "EscapingPrint: failed to write &lt;");
        }
        return 4;
      case '>':
        if(dest.print("&gt;") != 4){
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "EscapingPrint: failed to write &gt;");
        } 
        return 4;
      case '"':
        if(dest.print("&quot;") != 6){
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "EscapingPrint: failed to write &quot;");
        }
        return 6;
      case '\'':
        if(dest.print("&apos;") != 6){
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "EscapingPrint: failed to write &apos;");
        } 
        return 6;
      default:
        if(dest.write(&c, 1) != 1){
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "EscapingPrint: failed to write char %c", c);
        }
        return 1;
    }
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    size_t r = 0;
    for (size_t i = 0; i < size; ++i) {
      r += write(buffer[i]);
    }
    return r;
  }

 protected:
  Print& dest;
};

}  // namespace tiny_dlna
