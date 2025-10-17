#pragma once

namespace tiny_dlna {

/// @brief Media item description used to build DIDL-Lite entries
struct MediaItem {
  const char* id = nullptr;
  const char* parentID = "0";
  bool restricted = true;
  const char* title = nullptr;
  const char* res = nullptr;  // resource URL
  const char* mimeType = nullptr;
  // Additional optional metadata fields could be added here (duration,
  // creator...)
};

}  // namespace tiny_dlna
