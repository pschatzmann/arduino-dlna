#pragma once

#include "HttpHeader.h"
#include "HttpLineReader.h"
#include "basic/Logger.h"

namespace tiny_dlna {

/**
 * @brief Http might reply with chunks. So we need to dechunk the data.
 * see https://en.wikipedia.org/wiki/Chunked_transfer_encoding
 */
class HttpChunkReader : public HttpLineReader {
 public:
  /// default constructor
  HttpChunkReader() {
    open_chunk_len = 0;
    has_ended = false;
  }

  /// constructor for processing final header information
  HttpChunkReader(HttpReplyHeader &header) {
    http_heaer_ptr = &header;
    open_chunk_len = 0;
    has_ended = false;
  }

  void open(Client &client) {
    DlnaLogger.log(DlnaDebug, "HttpChunkReader %s", "open");
    has_ended = false;
    readChunkLen(client);
  }

  // reads a block of data from the chunks
  virtual int read(Client &client, uint8_t *str, int len) {
    DlnaLogger.log(DlnaDebug, "HttpChunkReader %s", "read");
    if (has_ended && open_chunk_len == 0) return 0;

    // read the chunk data - but not more then available
    int read_max = len < open_chunk_len ? len : open_chunk_len;
    int len_processed = client.read(str, read_max);
    // update current unprocessed chunk
    open_chunk_len -= len_processed;

    // remove traling CR LF from data
    if (open_chunk_len <= 0) {
      removeCRLF(client);
      readChunkLen(client);
    }

    return len_processed;
  }

  // reads a single line from the chunks
  virtual int readln(Client &client, uint8_t *str, int len,
                     bool incl_nl = true) {
    DlnaLogger.log(DlnaDebug, "HttpChunkReader %s", "readln");
    if (has_ended && open_chunk_len == 0) return 0;

    int read_max = len < open_chunk_len ? len : open_chunk_len;
    int len_processed = readlnInternal(client, str, read_max, incl_nl);
    open_chunk_len -= len_processed;

    // the chunks are terminated by a final CRLF
    if (open_chunk_len <= 0) {
      removeCRLF(client);
      readChunkLen(client);
    }

    return len_processed;
  }

  int available() {
    int result = has_ended ? 0 : open_chunk_len;
    DlnaLogger.log(DlnaDebug, "HttpChunkReader available=>%d", result);

    return result;
  }

 protected:
  int open_chunk_len;
  bool has_ended = false;
  HttpReplyHeader *http_heaer_ptr;

  void removeCRLF(Client &client) {
    DlnaLogger.log(DlnaDebug, "HttpChunkReader %s", "removeCRLF");
    // remove traling CR LF from data
    if (client.peek() == '\r') {
      DlnaLogger.log(DlnaDebug, "HttpChunkReader %s", "removeCR");
      client.read();
    }
    if (client.peek() == '\n') {
      DlnaLogger.log(DlnaDebug, "HttpChunkReader %s", "removeLF");
      client.read();
    }
  }

  // we read the chunk length which is indicated as hex value
  virtual void readChunkLen(Client &client) {
    DlnaLogger.log(DlnaDebug, "HttpChunkReader::readChunkLen");
    uint8_t len_str[51];
    readlnInternal(client, len_str, 50, false);
    DlnaLogger.log(DlnaDebug, "HttpChunkReader::readChunkLen %s",
                   (const char *)len_str);
    DlnaLogger.log(DlnaDebug, "\n");
    open_chunk_len = strtol((char *)len_str, nullptr, 16);

    char msg[40];
    sprintf(msg, "chunk_len: %d", open_chunk_len);
    DlnaLogger.log(DlnaDebug, "HttpChunkReader::readChunkLen->%s", msg);

    if (open_chunk_len == 0) {
      has_ended = true;
      DlnaLogger.log(DlnaDebug, "HttpChunkReader::readChunkLen %s",
                     "last chunk received");
      // processing of additinal final headers after the chunk end
      if (http_heaer_ptr != nullptr) {
        http_heaer_ptr->readExt(client);
      }
    }
  }
};

}  // namespace tiny_dlna
