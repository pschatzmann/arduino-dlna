#pragma once

#include "http/Http.h"
#include "http/Server/IHttpServer.h"
#include "dlna/Action.h"
#include "basic/Str.h"
#include "basic/StrView.h"

namespace tiny_dlna {

typedef void (*http_callback)(IHttpServer* server, const char* requestPath,
                              HttpRequestHandlerLine* hl);

/**
 * @brief Attributes needed for the DLNA Service Definition
 * @author Phil Schatzmann
 */
class DLNAServiceInfo {
 public:
  DLNAServiceInfo(bool flag = true) { is_active = flag; }

  /// Setup all relevant values
  void setup(const char* type, const char* id, const char* scp,
             http_callback cbScp, const char* control, http_callback cbControl,
             const char* event, http_callback cbEvent) {
    DlnaLogger.log(DlnaLogLevel::Info, "Setting up: %s | %s | %s", scp, control, event);

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
  const char* event_sub_sid = nullptr; /**< SID assigned by remote service (if subscribed) */
  http_callback scp_cb = nullptr;
  http_callback control_cb = nullptr;
  http_callback event_sub_cb = nullptr;

  /// for subscriptions
  Str subscription_id;
  SubscriptionState subscription_state = SubscriptionState::Unsubscribed;
  uint64_t time_subscription_started = 0;  ///< timestamp when subscription started
  uint64_t time_subscription_confirmed = 0;  ///< timestamp when subscription was confirmed
  uint64_t time_subscription_expires = 0;  ///< timestamp when subscription expires
  const char* subscription_namespace_abbrev = nullptr;

  // instance id
  int  instance_id = 0;
 
  // active/inactive
  bool is_active = true;
  operator bool() { return is_active; }

};

}  // namespace tiny_dlna