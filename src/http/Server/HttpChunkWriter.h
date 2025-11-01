#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Client.h"
#include "Print.h"
#include "basic/Logger.h"
#include "basic/StrView.h"

namespace tiny_dlna {

/**
 * @brief Writes the data chunked to the actual client
 */
class HttpChunkWriter {
 public:
  int writeChunk(Client& client, const char* str, int len,
                 const char* str1 = nullptr, int len1 = 0) {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpChunkWriter", "writeChunk");
    client.println(len + len1, HEX);
    int result = client.write((const uint8_t*)str, len);
    if (str1 != nullptr) {
      result += client.write((const uint8_t*)str1, len1);
    }
    client.println();
    return result;
  }

  int writeChunk(Client& client, const char* str) {
    int len = strlen(str);
    return writeChunk(client, str, len);
  }

  void writeEnd(Client& client) { writeChunk(client, "", 0); }
};

/**
 * @brief Print implementation for HTTP chunked transfer encoding
 *
 * This class wraps a Client and uses HttpChunkWriter to send data using
 * HTTP chunked transfer encoding. All writes are sent as individual chunks.
 * Provides print, println, printf, and printEscaped utilities for chunked output.
 *
 * Usage:
 *   ChunkPrint cp(client);
 *   cp.print("Hello");
 *   cp.println("World");
 *   cp.end(); // signals end of chunked stream
 *
 * Used internally for streaming large HTTP responses without buffering.
 */
class ChunkPrint : public Print {
 public:
  ChunkPrint(Client& client) : client_ptr(&client) {}

  size_t write(uint8_t ch) override {
    char c = (char)ch;
    chunk_writer.writeChunk(*client_ptr, &c, 1);
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (size == 0) return 0;
    chunk_writer.writeChunk(*client_ptr, (const char*)buffer, (int)size);
    return size;
  }

  size_t print(const char* str) {
    if (!str) return 0;
    int len = strlen(str);
    // avoid emitting a zero-length chunk mid-response (which would signal
    // the end of the chunked stream). If the string is empty, do nothing.
    if (len == 0) return 0;
    chunk_writer.writeChunk(*client_ptr, str, len);
    return len;
  }

  size_t printEscaped(const char* str) {
    if (!str) return 0;
    int len = strlen(str);
    if (len == 0) return 0;
    int max_len = len * 3 + 1;  // worst-case expansion when replacing to &amp;
    // allocate on heap to avoid large stack usage
    char* buffer = new char[max_len];
    // copy and perform replacements using StrView helper
    strncpy(buffer, str, max_len - 1);
    buffer[max_len - 1] = '\0';
    StrView str_view{buffer, max_len, (int)strlen(buffer)};
    str_view.replaceAll("&", "&amp;");
    str_view.replaceAll("<", "&lt;");
    str_view.replaceAll(">", "&gt;");
    int new_len = str_view.length();
    chunk_writer.writeChunk(*client_ptr, str_view.c_str(), new_len);
    delete[] buffer;
    return len;
  }

  size_t println(const char* str) {
    int len = strlen(str);
    // allocate on heap to avoid large VLA on stack
    char* tmp = new char[len + 3];
    strcpy(tmp, str);
    strcat(tmp, "\r\n");
    chunk_writer.writeChunk(*client_ptr, tmp, len + 2);
    delete[] tmp;
    return len + 2;
  }

  size_t println() {
    chunk_writer.writeChunk(*client_ptr, "\r\n", 2);
    return 2;
  }

  size_t printf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return 0;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    chunk_writer.writeChunk(*client_ptr, buf, n);
    return (size_t)n;
  }

  void end() { chunk_writer.writeEnd(*client_ptr); }

 protected:
  Client* client_ptr;
  HttpChunkWriter chunk_writer;
};

}  // namespace tiny_dlna
