#pragma once

#include "dlna/DLNACommon.h"

namespace tiny_dlna {
/**
 * @brief Abstract DLNA Descriptor Generation
 */
class DLNADescr {
 public:
  virtual size_t printDescr(Print& out) = 0;
};
}  // namespace tiny_dlna
