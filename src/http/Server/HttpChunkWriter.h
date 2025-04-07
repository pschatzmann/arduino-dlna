#pragma once

#include "Client.h"
#include "basic/Logger.h"

namespace tiny_dlna {

/**
 * @brief Writes the data chunked to the actual client
 *
 */
class HttpChunkWriter {
 public:
  int writeChunk(Client& client, const char* str, int len,
                 const char* str1 = nullptr, int len1 = 0) {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpChunkWriter", "writeChunk");
    client.println(len + len1, HEX);
    int result = client.write((uint8_t*)str, len);
    if (str1 != nullptr) {
      result += client.write((uint8_t*)str1, len1);
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

}  // namespace tiny_dlna
