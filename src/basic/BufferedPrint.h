#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "Print.h"

namespace tiny_dlna {

/**
 * @brief Print decorator which collects small writes into a buffer
 *
 * Network clients send each write() as a separate TCP packet when Nagle's
 * algorithm is disabled: writing an XML document character by character (e.g.
 * via EscapingPrint) is then extremely slow. This class forwards the data in
 * blocks of the indicated size. Call flush() at the end to send the rest.
 *
 * write() always reports the logical number of bytes, so that the caller can
 * compare it with a precalculated Content-Length. If the destination fails to
 * accept the data (e.g. the client closed the connection), all further data is
 * dropped: check hasError().
 */
class BufferedPrint : public Print {
 public:
  BufferedPrint(Print& dest, size_t size = 1024) : dest(dest) {
    buffer.resize(size > 0 ? size : 1);
  }

  ~BufferedPrint() { flush(); }

  size_t write(uint8_t ch) override {
    if (len >= buffer.size()) flush();
    buffer[len++] = ch;
    return 1;
  }

  size_t write(const uint8_t* data, size_t size) override {
    size_t remaining = size;
    while (remaining > 0) {
      if (len >= buffer.size()) flush();
      size_t n = buffer.size() - len;
      if (n > remaining) n = remaining;
      memcpy(buffer.data() + len, data, n);
      len += n;
      data += n;
      remaining -= n;
    }
    return size;
  }

  /// Sends the buffered data to the destination
  void flush() override {
    if (len == 0) return;
    if (!is_error) {
      size_t written = dest.write(buffer.data(), len);
      if (written != len) is_error = true;
    }
    len = 0;
  }

  /// true if the destination did not accept all data
  bool hasError() const { return is_error; }

 protected:
  Print& dest;
  std::vector<uint8_t> buffer;
  size_t len = 0;
  bool is_error = false;
};

}  // namespace tiny_dlna
