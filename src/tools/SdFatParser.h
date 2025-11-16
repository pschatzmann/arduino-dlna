#pragma once
/**
 * @file SdFatParser.h
 * @brief Lightweight line parser for SdFat directory listings.
 *
 * This header defines a tiny utility that consumes characters (typically the
 * textual output of an SdFat directory listing) and emits structured file
 * information via a user-provided callback. It derives from Arduino's
 * Print so it can be used anywhere a Print sink is accepted.
 */

#include <string>
#include <Print.h>

namespace tiny_dlna {

/**
 * @brief Describes an entry from a parsed SdFat directory listing.
 */
struct SdFatFileInfo {
  /** Fully qualified or relative name as seen in the listing.
   *  When the entry represents a directory, the name will end with '/'. */
  std::string name;
  /** True when the entry is a directory (name ends with '/'). */
  bool is_directory;
  /** Nesting depth inferred from leading spaces in the listing.
   *  Each two spaces are treated as one level. Root level is 0. */
  int level;
};

/**
 * @brief Simple character-by-character parser that emits SdFatFileInfo via a callback.
 *
 * Typical usage:
 * - Create an instance.
 * - Provide a callback with setCallback().
 * - Stream characters into the instance using Print::write() or print().
 *
 * The parser treats each newline ('\n') as the end of one listing line. It
 * uses the number of leading spaces to compute the nesting level (2 spaces = 1
 * level). A trailing '/' marks a directory entry.
 */
class SdFatParser : public Print {
public:
  /**
   * @brief Constructs the parser and reserves internal buffer capacity.
   */
  SdFatParser() {
    name.reserve(80);
  }

  /**
   * @brief Consumes one character of input.
   * @param c The character to process.
   * @return Always returns 1 to conform to Print::write contract.
   *
   * Processing rules:
   * - On '\n': finishes the current line and triggers parsing/callback.
   * - On '\t': ignored (tabs are skipped).
   * - Otherwise: appended to the current line buffer.
   */
  size_t write(uint8_t c) override {
    switch (c) {
      case '\n':
        parse();
        break;
      case '\t':
        break;
      default:
        name += c;
        break;
    }
    return 1;
  }

  /**
   * @brief Sets the callback that receives parsed entries.
   * @param cb A function pointer taking an SdFatFileInfo& and user reference.
   *           The callback may be null to disable notifications.
   * @param ref Opaque user data passed back to the callback.
   */
  void setCallback(void (*cb)(SdFatFileInfo&, void* ref), void* ref = nullptr) {
    this->ref = ref;
    this->cb = cb;
  }

protected:
  /** Accumulates characters until a newline terminates the line. */
  std::string name;
  /** User-provided callback invoked after each parsed line. */
  void (*cb)(SdFatFileInfo&, void* ref) = nullptr;
  /** Opaque user pointer returned to the callback. */
  void* ref = nullptr;
  /** Reused struct populated for each parsed entry to minimize allocations. */
  SdFatFileInfo info;

  /**
   * @brief Counts leading spaces in the current line buffer.
   * @return Number of consecutive spaces from the beginning of the buffer.
   */
  int spaceCount() {
    for (int j = 0; j < static_cast<int>(name.size()); j++) {
      if (name[j] != ' ') return j;
    }
    return 0;
  }

  /**
   * @brief Parses the accumulated line and emits a callback if set.
   *
   * Determines nesting level from leading spaces (2 spaces == 1 level), strips
   * the indentation, infers directory status from a trailing '/', and notifies
   * the user callback.
  */
  void parse() {
    int spaces = spaceCount();
    info.level = spaces / 2;
    info.name = name.erase(0, static_cast<size_t>(spaces));
    info.is_directory = !info.name.empty() && info.name[info.name.size() - 1] == '/';
    if (cb) cb(info, ref);
    name.clear();
  }
};

}  // namespace tiny_dlna
