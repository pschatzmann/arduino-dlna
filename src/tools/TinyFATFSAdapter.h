#pragma once

#include <stdint.h>

#include <string>

#include "Print.h"
// TinyFATFS: https://github.com/pschatzmann/TinyFATFS
#include "fatfs.h"
#include "filesystem.h"

namespace tiny_dlna {

/**
 * @brief Makes a TinyFATFS volume usable with SdFatContentProvider
 *
 * SdFatContentProvider builds its directory tree from the recursive listing
 * provided by SdFat's ls(). This adapter provides the same chdir()/ls() API for
 * a [TinyFATFS](https://github.com/pschatzmann/TinyFATFS) volume (fatfs::SDClass)
 * with any of its drivers (SPI, SDMMC, RAM, ...). It is available on all
 * platforms supported by TinyFATFS.
 *
 * The listing uses the FatFs directory functions (f_opendir/f_readdir), which
 * provide the name and the directory flag of each entry without opening it.
 *
 * Usage:
 * @code
 * #include "fatfs.h"
 * #include "tools/TinyFATFSAdapter.h"
 *
 * fatfs::SDClass sd;
 * TinyFATFSAdapter fs_adapter(sd);
 * SdFatContentProvider<TinyFATFSAdapter> contentProvider;
 * ...
 * sd.begin(CS, SPI);
 * contentProvider.begin(fs_adapter, "/");
 * ...
 * fatfs::File file = fs_adapter.open(node.path().c_str());
 * @endcode
 */
class TinyFATFSAdapter {
 public:
  TinyFATFSAdapter(fatfs::SDClass& sd) : p_sd(&sd) {}

  /// Defines the directory which is listed by ls()
  bool chdir(const char* path) {
    if (path == nullptr || *path == 0) path = "/";
    // the root directory has no FILINFO: f_stat() fails for "/"
    if (!isRoot(path)) {
      FILINFO info;
      if (p_sd->getFatFs()->f_stat(path, &info) != FR_OK) return false;
      if ((info.fattrib & AM_DIR) == 0) return false;
    }
    cwd = path;
    return true;
  }

  /// Lists the current directory recursively in the format of SdFat's ls():
  /// one entry per line, indented by 2 spaces per level, directories are
  /// marked with a trailing "/". The flags are ignored: the listing is always
  /// recursive.
  void ls(Print* out, uint8_t flags = 0) {
    if (out == nullptr) return;
    listDir(*out, cwd, 0);
  }

  /// Opens a file (e.g. to serve it)
  fatfs::File open(const char* path, uint8_t mode = FILE_READ) {
    return p_sd->open(path, mode);
  }

  /// Provides the wrapped volume
  fatfs::SDClass& sd() { return *p_sd; }

 protected:
  fatfs::SDClass* p_sd = nullptr;
  std::string cwd = "/";

  static bool isRoot(const char* path) {
    return path[0] == 0 || (path[0] == '/' && path[1] == 0);
  }

  /// Lists dir and (depth first) its subdirectories: the directory of each
  /// level stays open while its subdirectories are listed
  void listDir(Print& out, const std::string& dir, int level) {
    for (fatfs::directory_iterator it(p_sd->getFatFs(), dir);
         it != fatfs::directory_iterator::end(); ++it) {
      fatfs::directory_entry entry = *it;
      const char* name = nameOf(entry.path);
      for (int j = 0; j < level; j++) out.print("  ");
      out.print(name);
      if (entry.is_directory) {
        out.println("/");
        listDir(out, entry.path, level + 1);
      } else {
        out.println();
      }
    }
  }

  static const char* nameOf(const std::string& path) {
    size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? path.c_str() : path.c_str() + pos + 1;
  }
};

}  // namespace tiny_dlna
