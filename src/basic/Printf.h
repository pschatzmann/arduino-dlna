#pragma once
#include "Print.h"
#include "dlna_config.h"
namespace tiny_dlna {

/**
 * @brief Printf support with output to Print. This class does not do
 * any heap allocations!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Printf {
 public:
  Printf(Print& out) { p_print = &out; }

  size_t printf(const char* fmt, ...) {
    char buf[MAX_PRINTF_SIZE];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0) return 0;
    // vsnprintf returns the number of characters that would have been
    // written if the buffer were large enough. Clamp to the actual buffer
    // size to avoid reading past 'buf' when n >= sizeof(buf).
    int toWrite = n;
    if (toWrite >= (int)sizeof(buf)) toWrite = (int)sizeof(buf) - 1;
    p_print->write((const uint8_t*)buf, (size_t)toWrite);
    return (size_t)toWrite;
  }

 protected:
  Print* p_print = nullptr;
};

}  // namespace tiny_dlna