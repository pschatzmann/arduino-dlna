#pragma once
#include "TinyHttp/HttpServer.h"

namespace tiny_dlna {

typedef void (*http_callback)(HttpServer* server, const char* requestPath, HttpRequestHandlerLine* hl);

/**
 * @brief Attributes needed for the DLNA Service Definition
 */
class Service {
public:
  //Service() = default;
  //Service(&Service) = default;
  void setup(const char* key, const char* type, const char* id, const char* scp, http_callback cbScp,
          const char* control, http_callback cbControl, const char* event, http_callback cbEvent) {
    name = key;
    service_type = type;
    service_id = id;
    scp_url = scp;
    control_url = control;
    event_sub_url = event;
    scp_cb = cbScp;
    control_cb = cbControl;
    event_sub_cb = cbEvent;
  }
  const char* name = nullptr;
  const char* service_type = nullptr;
  const char* service_id = nullptr;
  const char* scp_url = nullptr;
  const char* control_url = nullptr;
  const char* event_sub_url = nullptr;
  http_callback scp_cb = nullptr;
  http_callback control_cb = nullptr;
  http_callback event_sub_cb = nullptr; 
};

} // ns