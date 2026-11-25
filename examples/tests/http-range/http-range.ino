/**
 * @file http-range.ino
 * @brief Unit test for HttpRange: parsing of the HTTP "Range" request header
 *
 * Prints one line per case and a summary. On the desktop the process exits
 * with 0 if all cases pass and 1 otherwise.
 */
#include <Arduino.h>

#include "http/Server/HttpRange.h"

using namespace tiny_dlna;

int failures = 0;
int count = 0;

/// expect a satisfiable range [start, end]
void expectRange(const char* header, uint64_t total, uint64_t start,
                 uint64_t end) {
  HttpRange range;
  bool ok = range.parse(header, total) && range.isSatisfiable() &&
            range.start() == start && range.end() == end &&
            range.length() == end - start + 1;
  count++;
  if (!ok) failures++;
  Serial.print(ok ? "PASS " : "FAIL ");
  Serial.print(header ? header : "(null)");
  Serial.print(" / ");
  Serial.print((unsigned long)total);
  Serial.print(" -> ");
  Serial.print((unsigned long)range.start());
  Serial.print("-");
  Serial.println((unsigned long)range.end());
}

/// expect a valid range which can not be satisfied (416)
void expectNotSatisfiable(const char* header, uint64_t total) {
  HttpRange range;
  bool ok = range.parse(header, total) && range.isValid() &&
            !range.isSatisfiable() && range.length() == 0;
  count++;
  if (!ok) failures++;
  Serial.print(ok ? "PASS " : "FAIL ");
  Serial.print(header);
  Serial.print(" / ");
  Serial.print((unsigned long)total);
  Serial.println(" -> 416");
}

/// expect the header to be ignored (reply with the full content)
void expectIgnored(const char* header, uint64_t total) {
  HttpRange range;
  bool ok = !range.parse(header, total) && !range.isSatisfiable();
  count++;
  if (!ok) failures++;
  Serial.print(ok ? "PASS " : "FAIL ");
  Serial.print(header ? header : "(null)");
  Serial.println(" -> ignored (200)");
}

void setup() {
  Serial.begin(115200);

  // regular ranges
  expectRange("bytes=0-99", 1000, 0, 99);
  expectRange("bytes=0-0", 1000, 0, 0);
  expectRange("bytes=100-", 1000, 100, 999);
  expectRange("bytes=0-", 1000, 0, 999);
  expectRange("bytes=999-", 1000, 999, 999);
  expectRange("bytes=990-2000", 1000, 990, 999);  // end is clamped
  expectRange("bytes=-100", 1000, 900, 999);      // suffix range
  expectRange("bytes=-5000", 1000, 0, 999);       // suffix longer than content
  expectRange(" bytes=1-2", 1000, 1, 2);          // leading space
  expectRange("bytes=1-2 ", 1000, 1, 2);          // trailing space
  // positions above 4 GB
  expectRange("bytes=4294967296-", 5000000000ULL, 4294967296ULL,
              4999999999ULL);

  // valid but not satisfiable: 416
  expectNotSatisfiable("bytes=1000-", 1000);
  expectNotSatisfiable("bytes=5000-6000", 1000);
  expectNotSatisfiable("bytes=-0", 1000);
  expectNotSatisfiable("bytes=0-", 0);

  // ignored: reply with the full content
  expectIgnored(nullptr, 1000);
  expectIgnored("", 1000);
  expectIgnored("bytes=0-1,5-6", 1000);  // multiple ranges
  expectIgnored("items=0-1", 1000);      // unknown unit
  expectIgnored("bytes=abc", 1000);
  expectIgnored("bytes=a-5", 1000);
  expectIgnored("bytes=1-b", 1000);
  expectIgnored("bytes=-", 1000);
  expectIgnored("bytes=5-2", 1000);  // last < first

  Serial.print(count - failures);
  Serial.print(" of ");
  Serial.print(count);
  Serial.println(failures == 0 ? " tests passed" : " tests passed: FAILED");
#ifdef IS_DESKTOP
  exit(failures == 0 ? 0 : 1);
#endif
}

void loop() { delay(1000); }
