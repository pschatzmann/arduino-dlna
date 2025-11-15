#pragma once
#include "Print.h"

namespace tiny_dlna {

/**
 * @brief Class with does not do any output: it can be used to determine
 * the length of the output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NullPrint : public Print {
 public:
  size_t write(uint8_t ch) override { return 1; }
  size_t write(const uint8_t* buffer, size_t size) override { return size; }
  
};

}  // namespace tiny_dlna