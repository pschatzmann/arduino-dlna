#pragma once
#include "HttpRequest.h"
#include "basic/Url.h"

namespace tiny_dlna {

/**
 * @brief Forwards a request to a destination URL and provides a pointer to the
 * result stream
 *
 */
class HttpTunnel {
 public:
  HttpTunnel(const char* url, const char* mime = "text/html") {
    DlnaLogger.log(DlnaInfo, "HttpTunnel: %s", url);
    v_url.setUrl(url);
    v_mime = mime;
  }

  /// Executes the get request
  Stream* get() {
    DlnaLogger.log(DlnaInfo, "HttpTunnel::get");
    if (isOk(v_request.get(v_url, v_mime))) {
      return v_request.client();
    }
    return nullptr;
  }

  HttpRequest& request() { return v_request; }

  const char* mime() { return v_mime; }

 protected:
  Url v_url;
  HttpRequest v_request;
  const char* v_mime;

  bool isOk(int code) { return code >= 200 && code < 300; }
};

}  // namespace tiny_dlna