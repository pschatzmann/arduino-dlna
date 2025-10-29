
#pragma once
#include "icons/icon48x48.h"

namespace tiny_dlna {
/**
 * @brief Information about the icon
 * @author Phil Schatzmann
*/
class Icon {
 public:
  const char* mime = "image/png";
  int width = 48;
  int height = 48;
  int depth = 8;
  const char* icon_url = "/icon.png";
  uint8_t* icon_data = (uint8_t*) icon_png;
  int icon_size = icon_png_len;
};

}