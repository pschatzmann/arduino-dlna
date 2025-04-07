#pragma once

#include "HttpChunkWriter.h"
#include "HttpStreamedOutput.h"
#include "WiFi.h"
#include "basic/List.h"
#include "basic/Str.h"

namespace tiny_dlna {

/**
 * @brief Generic HttpStreamedMultiOutput Class which handles multicast
 * streaming. We can optionally define some default (e.g. html) content that
 * will be written at the beginning and at the end of the streaming session to
 * the client.
 *
 * The output (write, print ...) functions are sending the same output to all
 * open clients.
 *
 * The id is used to identify the output stream so that we can potentially send
 * different content to different clients.
 *
 * We automatically manage all the clients which are open and clean up the
 * closed clients to release the memory.
 */

class HttpStreamedMultiOutput : public HttpStreamedOutput {
 public:
  HttpStreamedMultiOutput(const char *mime, const char *startHtml = nullptr,
                          const char *endHtml = nullptr,
                          int maxHistoryLength = 0) {
    DlnaLogger.log(DlnaLogLevel::Info, "HttpStreamedMultiOutput");
    this->start = startHtml;
    this->end = endHtml;
    this->mime_type = mime;
    this->max_history_length = maxHistoryLength;
    if (maxHistoryLength > 0) {
      this->history = new Str(maxHistoryLength);
    }
  }

  // provides the mime type
  virtual const char *mime() { return mime_type; }

  // checks if the client is valid
  virtual bool isValid(WiFiClient &client) {
    bool valid = client.connected();
    return valid;
  }

  // content that is written when the request is opened
  virtual void open(WiFiClient &client) {
    DlnaLogger.log(DlnaLogLevel::Info, "HttpStreamedMultiOutput::open");
    if (client.connected()) {
      // create a copy
      // we handle only valid clents
      if (start != nullptr) {
        int len = strlen(start);
        writer.writeChunk(client, start, len);
      }
      if (history != nullptr && history->length() > 0) {
        writer.writeChunk(client, history->c_str(), history->length());
      }

      // add client to list of open clients
      DlnaLogger.log(DlnaLogLevel::Warning, "new client");
      clients.push_back(client);
    }
  }

  // checks if we have any open clients
  virtual bool isOpen() {
    cleanup();
    for (auto i = clients.begin(); i != clients.end(); ++i) {
      WiFiClient client = (*i);
      if (isValid(client)) {
        return true;
      }
    }
    return false;
  }

  // end processing by wr
  virtual void close() {
    for (auto i = clients.begin(); i != clients.end(); ++i) {
      WiFiClient client = *i;
      if (isValid(client)) {
        if (end != nullptr) {
          // send end to all clients
          print(end);
        }
        writer.writeEnd(client);
      }
    }
    cleanup();
  }

  /// Do not accept any writes if we are not connected
  virtual int availableForWrite() { return isOpen() ? 1024 : 0; }

  // write the content to the HttpStreamedMultiOutput
  virtual size_t write(uint8_t *content, int len) {
    cleanup();
    for (auto i = clients.begin(); i != clients.end(); ++i) {
      WiFiClient client = *i;
      if (isValid(client)) {
        DlnaLogger.log(DlnaLogLevel::Debug, "HttpStreamedMultiOutput::write");
        writer.writeChunk(client, (const char *)content, len);
      }
    }
    return len;
  }

  // writes a line
  virtual size_t print(const char *str) {
    cleanup();
    int len = strlen(str);
    for (auto i = clients.begin(); i != clients.end(); ++i) {
      WiFiClient client = *i;
      if (isValid(client)) {
        DlnaLogger.log(DlnaLogLevel::Debug, "HttpStreamedMultiOutput::print");
        writer.writeChunk(client, (const char *)str, len);
      }
    }
    addHistory(str, false, len);
    return len;
  }

  // writes a line which terminates with a html line break
  virtual size_t println(const char *str) {
    cleanup();
    int len = strlen(str);
    for (auto i = clients.begin(); i != clients.end(); ++i) {
      WiFiClient client = *i;
      if (isValid(client)) {
        DlnaLogger.log(DlnaLogLevel::Debug, "HttpStreamedMultiOutput::println");
        writer.writeChunk(client, str, len, "<br>", 4);
      }
    }
    addHistory(str, true, len);
    return len;
  }

  // actually we do nothing here - but some subclasses might
  virtual void doLoop() {}

 protected:
  HttpChunkWriter writer;
  List<WiFiClient> clients;
  Str *history = nullptr;
  int max_history_length;
  const char *start = nullptr;
  const char *end = nullptr;
  const char *mime_type = nullptr;
  int id_value = 0;

  // clenaup closed clients
  void cleanup() {
    for (int pos = clients.size() - 1; pos >= 0; pos--) {
      WiFiClient client = clients[pos];
      if (!isValid(client)) {
        DlnaLogger.log(DlnaLogLevel::Warning, "HttpStreamedMultiOutput::closed");
        clients.erase(clients.begin() + pos);
      }
    }
  }

  /// content that is written when the request is opened
  void onClose(WiFiClient &client) {
    if (end != nullptr) {
      DlnaLogger.log(DlnaLogLevel::Info, "HttpStreamedMultiOutput::onClose");
      int len = strlen(end);
      writer.writeChunk(client, end, len);
    }

    clients.push_back(client);
  }

  /// adds the line to the history - removest oldest lines
  void addHistory(const char *line, bool delimiter, int len) {
    if (history != nullptr) {
      int available = max_history_length - history->length();
      // make space
      while (len > available && history->length() > 0) {
        int pos = history->indexOf("<br>");
        if (pos >= 0) {
          *history << (pos + 4);
        } else {
          history->clear();
        }
        available = max_history_length - history->length();
      }
      // add to history if it is not too big
      if (len <= available) {
        *history += line;
        if (delimiter) {
          *history += "<br>";
        }
      }
    }
  }
};

}  // namespace tiny_dlna
