#pragma once
#include "Print.h"

namespace tiny_dlna {

/**
 * @brief Print wrapper that retries writing unwritten data.
 *
 * RetryPrint is a Print adapter that attempts to ensure all data is written to the underlying Print target.
 * If a write operation does not write all requested bytes (e.g., due to a transient failure or buffer full),
 * it will retry up to a configurable number of times, rewriting any unwritten data. Retries are reset on progress.
 *
 * Example usage:
 * @code
 *   WiFiClient client;
 *   RetryPrint retryPrint(client, 5); // Retry up to 5 times
 *   retryPrint.print("Hello, world!\n");
 * @endcode
 *
 * @note If used outside Arduino, ensure a suitable delay() implementation is available.
 */
class RetryPrint : public Print {
 public:
  RetryPrint(Print& out, int maxRetries = 3)
      : out(out), maxRetries(maxRetries) {}

  size_t write(uint8_t ch) override { return write(&ch, 1); }

  size_t write(const uint8_t* buffer, size_t size) override {
    size_t totalWritten = 0;
    int retries = 0;
    while (totalWritten < size && retries < maxRetries) {
      size_t written = out.write(buffer + totalWritten, size - totalWritten);
      if (written == 0) {
        ++retries;
        delay(10);  // Small delay before retry
      } else {
        totalWritten += written;
        retries = 0;  // Reset retries on progress
      }
    }
    return totalWritten;
  }

 protected:
  Print& out;
  int maxRetries;
};

}  // namespace tiny_dlna
