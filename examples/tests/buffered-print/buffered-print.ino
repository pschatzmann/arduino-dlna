/**
 * @file buffered-print.ino
 * @brief Unit test for BufferedPrint
 *
 * Verifies that small writes are collected into blocks, that the data arrives
 * unchanged and that write errors of the destination are reported. Prints one
 * line per case and a summary. On the desktop the process exits with 0 if all
 * cases pass and 1 otherwise.
 */
#include <Arduino.h>

#include <string>

#include "basic/BufferedPrint.h"
#include "basic/EscapingPrint.h"

using namespace tiny_dlna;

/// Destination which records the data and the number of write calls; it can
/// simulate a closed connection by accepting only a limited number of bytes
class RecordingPrint : public Print {
 public:
  RecordingPrint(size_t limit = SIZE_MAX) : limit(limit) {}
  size_t write(uint8_t ch) override { return write(&ch, 1); }
  size_t write(const uint8_t* buffer, size_t size) override {
    calls++;
    size_t n = size;
    if (data.size() + n > limit) n = limit - data.size();
    data.append((const char*)buffer, n);
    return n;
  }
  std::string data;
  int calls = 0;
  size_t limit;
};

int failures = 0;
int count = 0;

void check(bool ok, const char* name) {
  count++;
  if (!ok) failures++;
  Serial.print(ok ? "PASS " : "FAIL ");
  Serial.println(name);
}

std::string pattern(size_t len) {
  std::string result;
  for (size_t j = 0; j < len; j++) result += (char)('a' + j % 26);
  return result;
}

void testSmallWritesAreCollected() {
  RecordingPrint dest;
  BufferedPrint out(dest, 64);
  std::string expected = pattern(200);
  for (char c : expected) out.write((uint8_t)c);
  check(dest.calls == 3, "200 single byte writes: 3 full blocks before flush");
  out.flush();
  check(dest.calls == 4, "flush sends the remaining 8 bytes");
  check(dest.data == expected, "single byte writes: data unchanged");
}

void testBlockWrites() {
  RecordingPrint dest;
  BufferedPrint out(dest, 64);
  std::string expected = pattern(1000);
  size_t reported = 0;
  // mix of small and large writes
  reported += out.write((const uint8_t*)expected.data(), 10);
  reported += out.write((const uint8_t*)expected.data() + 10, 300);
  reported += out.write((const uint8_t*)expected.data() + 310, 690);
  out.flush();
  check(reported == 1000, "block writes report the logical size");
  check(dest.data == expected, "block writes: data unchanged");
  check(dest.calls == 16, "1000 bytes with buffer 64: 16 destination writes");
}

void testPrintAndEscaping() {
  RecordingPrint dest;
  {
    BufferedPrint out(dest, 1024);
    EscapingPrint esc(out);
    size_t len = out.print("<a>");
    len += esc.print("x<y & \"z\"");
    len += out.print("</a>");
    check(len == 3 + 26 + 4, "print returns the logical (escaped) size");
    check(dest.calls == 0, "nothing sent before flush");
  }  // destructor flushes
  check(dest.data == "<a>x&lt;y &amp; &quot;z&quot;</a>",
        "escaped XML: data unchanged");
  check(dest.calls == 1, "escaped XML sent with a single write");
}

void testFlushWhenEmpty() {
  RecordingPrint dest;
  BufferedPrint out(dest, 16);
  out.flush();
  out.flush();
  check(dest.calls == 0 && !out.hasError(), "flush without data: no write");
}

void testDestinationError() {
  RecordingPrint dest(100);  // e.g. client closed after 100 bytes
  BufferedPrint out(dest, 64);
  std::string data = pattern(300);
  size_t reported = out.write((const uint8_t*)data.data(), data.size());
  out.flush();
  check(reported == 300, "error: write still reports the logical size");
  check(out.hasError(), "error: hasError() after a short write");
  int calls = dest.calls;
  out.print("more data");
  out.flush();
  check(dest.calls == calls, "error: further data is dropped");
  check(dest.data == data.substr(0, 100), "error: accepted data unchanged");
}

void setup() {
  Serial.begin(115200);
  testSmallWritesAreCollected();
  testBlockWrites();
  testPrintAndEscaping();
  testFlushWhenEmpty();
  testDestinationError();

  Serial.print(count - failures);
  Serial.print(" of ");
  Serial.print(count);
  Serial.println(failures == 0 ? " tests passed" : " tests passed: FAILED");
#ifdef IS_DESKTOP
  exit(failures == 0 ? 0 : 1);
#endif
}

void loop() { delay(1000); }
