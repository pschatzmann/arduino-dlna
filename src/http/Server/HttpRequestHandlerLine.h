#pragma once

#include "HttpHeader.h"
#include "IHttpServer.h"

namespace tiny_dlna {

/**
 * @brief Used to register and process callbacks
 *
 */
class HttpRequestHandlerLine {
 public:
  HttpRequestHandlerLine(int ctxSize = 0) {
    DlnaLogger.log(DlnaLogLevel::Debug, "HttpRequestHandlerLine");
    contextCount = ctxSize;
    if (ctxSize > 0) {
      context = new void*[ctxSize];
      for (int i = 0; i < ctxSize; i++) {
        context[i] = nullptr;
      }
    } 
  }

  ~HttpRequestHandlerLine() {
    DlnaLogger.log(DlnaLogLevel::Debug, "~HttpRequestHandlerLine");
    if (contextCount > 0) {
      DlnaLogger.log(DlnaLogLevel::Debug, "HttpRequestHandlerLine %s", "free");
      if (contextCount > 0) delete[] context;
    }
  }

  TinyMethodID method;
  Str path;
  const char* mime = nullptr;
  web_callback_fn fn;
  void** context = nullptr;
  int contextCount = 0;
  StrView* header = nullptr;
};

}  // namespace tiny_dlna
