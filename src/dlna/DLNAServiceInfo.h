#pragma once

#include "http/Http.h"

#define DLNA_MAX_URL_LEN 120

namespace tiny_dlna {

typedef void (*http_callback)(HttpServer* server, const char* requestPath,
                              HttpRequestHandlerLine* hl);

/**
 * @brief Attributes needed for the DLNA Service Definition
 * @author Phil Schatzmann
 */
class DLNAServiceInfo {
 public:
  DLNAServiceInfo(bool flag = true) { is_active = flag; }
  void setup(const char* type, const char* id, const char* scp,
             http_callback cbScp, const char* control, http_callback cbControl,
             const char* event, http_callback cbEvent) {
    DlnaLogger.log(DlnaLogLevel::Info, "setting up: %s | %s | %s", scp, control, event);

    service_type = type;
    service_id = id;
    scpd_url = scp;
    control_url = control;
    event_sub_url = event;
    scp_cb = cbScp;
    control_cb = cbControl;
    event_sub_cb = cbEvent;
  }
  const char* service_type = nullptr;
  const char* service_id = nullptr;
  const char* scpd_url = nullptr;
  const char* control_url = nullptr;
  const char* event_sub_url = nullptr;
  http_callback scp_cb = nullptr;
  http_callback control_cb = nullptr;
  http_callback event_sub_cb = nullptr;
  bool is_active = true;
  operator bool() { return is_active; }
};

}  // namespace tiny_dlna