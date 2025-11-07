#pragma once

#include "HttpHeader.h"
#include "IHttpServer.h"

namespace tiny_dlna {

class IHttpServer;
class HttpRequestHandlerLine;

/**
 * @brief Used to register and process callbacks
 *
 */
class HttpRequestHandlerLine {
 public:
  HttpRequestHandlerLine(int ctxSize = 0) {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpRequestHandlerLine");
    contextCount = ctxSize;
    context = new void*[ctxSize];
  }

  ~HttpRequestHandlerLine() {
    DlnaLogger.log(DlnaLogLevel::Debug, "~HttpRequestHandlerLine");
    if (contextCount > 0) {
      DlnaLogger.log(DlnaLogLevel::Debug, "HttpRequestHandlerLine %s", "free");
      delete[] context;
    }
  }

  TinyMethodID method;
  Str path;
  const char* mime = nullptr;
  web_callback_fn fn;
  void** context;
  int contextCount;
  StrView* header = nullptr;
};

}  // namespace tiny_dlna
