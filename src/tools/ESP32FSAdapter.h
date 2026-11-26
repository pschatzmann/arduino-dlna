#pragma once

#ifdef ESP32

#include <FS.h>
#include <string.h>

#include <string>

namespace tiny_dlna {

/**
 * @brief Makes an ESP32 file system (fs::FS) usable with SdFatContentProvider
 *
 * SdFatContentProvider builds its directory tree from the recursive listing
 * provided by SdFat's ls(). This adapter provides the same chdir()/ls() API for
 * the file systems of the ESP32 core: e.g. SD_MMC, SD, LittleFS, FFat or
 * SPIFFS.
 *
 * The listing uses File::getNextFileName(&isDir), which only reads the
 * directory entries. File::openNextFile() would also open every file and
 * directory, which is considerably slower.
 *
 * Usage:
 * @code
 * #include "SD_MMC.h"
 * #include "tools/ESP32FSAdapter.h"
 *
 * ESP32FSAdapter fs_adapter(SD_MMC);
 * SdFatContentProvider<ESP32FSAdapter> contentProvider;
 * ...
 * SD_MMC.begin("/sdcard", true);
 * contentProvider.begin(fs_adapter, "/");
 * ...
 * File file = fs_adapter.open(node.path().c_str());
 * @endcode
 */
class ESP32FSAdapter {
 public:
  ESP32FSAdapter(fs::FS& fs) : p_fs(&fs) {}

  /// Defines the directory which is listed by ls()
  bool chdir(const char* path) {
    if (path == nullptr || *path == 0) path = "/";
    File dir = p_fs->open(path);
    if (!dir || !dir.isDirectory()) return false;
    cwd = path;
    return true;
  }

  /// Lists the current directory recursively in the format of SdFat's ls():
  /// one entry per line, indented by 2 spaces per level, directories are
  /// marked with a trailing "/". The flags are ignored: the listing is always
  /// recursive.
  void ls(Print* out, uint8_t flags = 0) {
    if (out == nullptr) return;
    listDir(*out, cwd.c_str(), 0);
  }

  /// Opens a file (e.g. to serve it)
  File open(const char* path, const char* mode = FILE_READ) {
    return p_fs->open(path, mode);
  }

  /// Provides the wrapped file system
  fs::FS& fs() { return *p_fs; }

 protected:
  fs::FS* p_fs = nullptr;
  std::string cwd = "/";

  void listDir(Print& out, const char* dir, int level) {
    File root = p_fs->open(dir);
    if (!root || !root.isDirectory()) return;
    bool is_dir = false;
    // Note: an empty result marks the end of the directory
    String path = root.getNextFileName(&is_dir);
    while (path.length() > 0) {
      const char* name = strrchr(path.c_str(), '/');
      name = name != nullptr ? name + 1 : path.c_str();
      for (int j = 0; j < level; j++) out.print("  ");
      out.print(name);
      if (is_dir) {
        out.println("/");
        listDir(out, path.c_str(), level + 1);
      } else {
        out.println();
      }
      path = root.getNextFileName(&is_dir);
    }
  }
};

}  // namespace tiny_dlna

#endif  // ESP32
