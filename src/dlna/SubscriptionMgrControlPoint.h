
#pragma once

#include <functional>

#include "DLNADeviceInfo.h"
#include "IUDPService.h"
#include "StringRegistry.h"
#include "SubscriptionMgrDevice.h"
#include "basic/Url.h"
#include "basic/Vector.h"
#include "dlna_config.h"
#include "http/Http.h"
#include "xml/XMLParserPrint.h"

namespace tiny_dlna {
/**
 * @class SubscriptionMgrControlPoint
 * @brief Standalone manager for UPnP/DLNA event subscriptions used by control
 *        points and devices.
 *
 * This class centralizes SUBSCRIBE / UNSUBSCRIBE lifecycle management and
 * NOTIFY handling for UPnP event subscriptions. It is intentionally
 * independent from DLNAControlPoint: the owner injects dependencies via
 * @c setup() and may store this manager as a value member (no dynamic
 * allocation required).
 *
 * Responsibilities:
 * - Perform SUBSCRIBE requests and store subscription metadata on
 *   DLNAServiceInfo (SID, subscription_state, timestamps).
 * - Perform UNSUBSCRIBE requests and clear local metadata.
 * - Register an HTTP notify handler and parse incoming NOTIFY messages
 *   incrementally using XMLParserPrint to keep RAM usage low.
 * - Invoke a user-provided callback for property change events.
 *
 * Thread / lifetime notes:
 * - The manager does not own injected pointers (HttpServer, DLNAHttpRequest,
 *   IUDPService, Url, StringRegistry, devices vector). The caller must
 *   ensure these outlive the manager.
 * - All public methods are safe to call from the single-threaded Arduino
 *   loop context. They are not thread-safe for concurrent access.
 */
class SubscriptionMgrControlPoint {
 public:
  /**
   * @brief Inject required dependencies and initialize the manager.
   *
   * The manager keeps non-owning pointers to the provided objects; the
   * caller must ensure the lifetime of these objects exceeds the manager's
   * lifetime.
   *
   * @param http Reference to the HTTP helper used to perform SUBSCRIBE/
   *             UNSUBSCRIBE requests.
   * @param udp Reference to the UDP discovery/service helper (not owned).
   * @param localCallbackUrl URL that remote services should use when
   *                         delivering NOTIFY messages (manager registers
   *                         an endpoint based on this path).
   * @param reg StringRegistry used for shared string storage (not owned).
   * @param devices Vector of DLNADeviceInfo instances managed by the
   *                control point; used to find matching services.
   */
  void setup(DLNAHttpRequest& http, IUDPService& udp, Url& localCallbackUrl,
             StringRegistry& reg, DLNADeviceInfo& device) {
    p_http = &http;
    p_udp = &udp;
    // store pointers to shared objects (caller/owner manages lifetime)
    p_local_url = &localCallbackUrl;
    p_device = &device;
    is_setup = true;
    // Initialize event subscription state
    setEventSubscriptionActive(event_subscription_active);
  }
  /**
   * @brief Attach an HttpServer instance and register the internal
   *        NOTIFY handler.
   *
   * @param server HttpServer used to receive NOTIFY callbacks.
   * @param port Optional port the server listens on (default 80). This is
   *             informational; the server instance controls actual binding.
   *
   * The method registers an HTTP handler at the local callback path
   * (derived from the injected @c Url via setup()).
   */
  void setHttpServer(HttpServer& server, int port = 80) {
    DlnaLogger.log(DlnaLogLevel::Debug, "DLNAControlPointMgr::setHttpServer");
    p_http_server = &server;
    http_server_port = port;
    attachHttpServer(server);
  }

  /**
   * @brief Return true when an application notification callback is set.
   */
  bool hasEventSubscriptionCallback() { return event_callback != nullptr; }

  /**
   * @brief Register a callback invoked when event properties change.
   *
   * @param callback Function with signature
   *   void callback(const char* sid, const char* varName,
   *                 const char* newValue, void* reference)
   *   - sid: Subscription ID string reported by the remote service (may be
   *     empty).
   *   - varName: Name of the changed state variable.
   *   - newValue: New textual value for the state variable.
   *   - reference: Opaque pointer provided by the caller via @p ref.
   * @param ref Opaque caller-provided pointer forwarded to the callback.
   */
  void setEventSubscriptionCallback(
      std::function<void(const char* sid, const char* varName,
                         const char* newValue, void* reference)>
          callback,
      void* ref = nullptr) {
    event_callback = callback;
    event_callback_ref = ref;
  }

  /**
   * @brief Tear down the manager. This clears the internal setup state but
   *        does not perform network unsubscribe calls.
   */
  void end() {
    // Reset pointers and flags that were initialized by setup()
    p_http = nullptr;
    p_udp = nullptr;
    p_local_url = nullptr;
    p_device = nullptr;
    // Clear manager state related to active subscriptions
    is_setup = false;
    subscription_state = SubscriptionState::Unsubscribed;
    subscribe_timeout = 0;
    last_event_notify_ms = 0;
    event_subscription_active = false;
    processing_timeout = 0;
    // leave p_http_server alone (owner may manage server lifecycle)
  }

  /**
   * @brief Configure desired subscription duration in seconds.
   *
   * This value will be used in the TIMEOUT header when issuing SUBSCRIBE
   * requests. The remote service may honor a shorter duration; the manager
   * tracks expiry via timestamps stored on the service info.
   *
   * @param seconds Desired subscription lifetime in seconds (default 3600).
   */
  void setEventSubscriptionDurationSec(int seconds = 3600) {
    event_subscription_duration_sec = seconds;
  }

  /**
   * @brief Set the retry threshold in milliseconds.
   *
   * If no NOTIFY messages are received for a subscribed service within this
   * interval, the manager may attempt re-subscription. A value of 0 disables
   * this behavior.
   *
   * @param ms Milliseconds to wait without NOTIFY before retry (0 = disabled).
   */
  void setEventSubscriptionRetryMs(int ms) { event_subscription_retry_ms = ms; }

  /**
   * @brief Turn automatic event subscription on or off.
   *
   * When enabled and the manager is setup and active the manager will
   * attempt to subscribe to services and renew existing subscriptions as
   * needed. When disabled the manager will attempt to unsubscribe from
   * services if currently subscribed.
   *
   * @param active true to maintain subscriptions automatically; false to
   *               stop and attempt unsubscription.
   */
  void setEventSubscriptionActive(bool active) {
    event_subscription_active = active;

    // if not setup, ignore
    if (!is_setup) return;

    updateSubscriptions();

    if (active) {
      is_active = true;
    }
  }

  /**
   *  @brief Get the timestamp of the last received NOTIFY message.
   */
  uint64_t getLastEventNotifyMs() { return last_event_notify_ms; }

  /**
   * @brief Periodic work to manage subscription renewals and retries.
   *
   * Call this from the main Arduino loop() if you want automatic renewal
   * and retry behavior. Returns true when the manager is active.
   *
   * @return true when the manager is active and loop processing occurred.
   */
  bool loop() {
    if (!isActive()) return false;
    if (!is_setup) return false;

    if (processing_timeout == 0 || millis() < processing_timeout) {
      updateSubscriptions();
      processing_timeout = millis() + event_subscription_retry_ms;
      return true;
    }
    return false;
  }

  /**
   * @brief Query whether the manager is currently active.
   *
   * Active means the manager is allowed to maintain subscriptions (it does
   * not imply that subscriptions currently exist).
   */
  bool isActive() { return is_active; }

 protected:
  void* event_callback_ref = nullptr;
  bool is_active = false;
  SubscriptionState subscription_state = SubscriptionState::Unsubscribed;
  bool is_setup = false;
  uint64_t subscribe_timeout = 0;
  uint32_t event_subscription_duration_sec = 3600;
  uint32_t event_subscription_retry_ms = 0;
  bool event_subscription_active = false;
  uint64_t last_event_notify_ms = 0;  // timestamp of last received NOTIFY
  DLNAHttpRequest* p_http = nullptr;
  IUDPService* p_udp = nullptr;
  HttpServer* p_http_server = nullptr;
  int http_server_port = 80;
  Url* p_local_url = nullptr;
  DLNADeviceInfo* p_device = nullptr;
  DLNAServiceInfo NO_SERVICE;
  uint64_t processing_timeout = 0;
  std::function<void(const char* sid, const char* varName, const char* newValue,
                     void* reference)>
      event_callback = [](const char* sid, const char* varName,
                          const char* newValue, void* reference) {
        // Default simple logger: print SID, variable name and new value.
        DlnaLogger.log(DlnaLogLevel::Info,
                       "- Event notification: SID='%s' var='%s' value='%s'",
                       sid ? sid : "", varName ? varName : "",
                       newValue ? newValue : "");
      };

  /**
   * @brief Evaluate and apply subscription state transitions.
   *
   * This helper inspects the manager-level flags and current
   * SubscriptionState and triggers subscribe/unsubscribe actions by
   * delegating to @c subscribeNotifications(). It implements the
   * high-level policy:
   * - If the manager should be maintaining subscriptions and there is no
   *   active subscription, trigger subscribing.
   * - If the manager should not maintain subscriptions but a subscription
   *   exists, trigger unsubscribing.
   *
   * The method does not parse NOTIFY payloads or manipulate per-service
   * metadata directly; those responsibilities remain in the per-service
   * subscribe/unsubscribe helpers and the notify handler.
   *
   * This method is invoked by @c setEventSubcriptionActive() and @c loop()
   * to drive periodic subscription maintenance.
   */
  void updateSubscriptions() {
    // subscribe
    if (subscription_state != SubscriptionState::Subscribed &&
        event_subscription_active) {
      subscription_state = SubscriptionState::Subscribing;
      subscribeNotifications(event_subscription_active);
    }
    // or unsubscribe
    if (subscription_state == SubscriptionState::Subscribed &&
        !event_subscription_active) {
      subscription_state = SubscriptionState::Unsubscribing;
      subscribeNotifications(event_subscription_active);
    }
  }

  /**
   * @brief Update internal and service metadata when a NOTIFY is received.
   *
   * This method should be called early when a NOTIFY is processed to reset
   * the last-received timestamp and mark the service as confirmed.
   *
   * @param sid Subscription ID (SID) extracted from the incoming NOTIFY
   *            request. If @p sid is nullptr or not found the call is a
   * no-op.
   */
  void updateReceived(const char* sid) {
    last_event_notify_ms = millis();
    DLNAServiceInfo& service = getServiceInfoBySID(sid);
    if (&service != &NO_SERVICE) {
      service.time_subscription_confirmed = millis();
      service.subscription_state = SubscriptionState::Subscribed;
    }
  }

  // Find service info by matching stored SID. Returns NO_SERVICE if not
  // found.
  DLNAServiceInfo& getServiceInfoBySID(const char* sid) {
    if (sid == nullptr) return NO_SERVICE;

    // Search single selected device first (if set)
    if (p_device != nullptr) {
      for (auto& service : p_device->getServices()) {
        const char* ssid = service.event_sub_sid;
        if (ssid != nullptr && strcmp(ssid, sid) == 0) return service;
      }
    }

    return NO_SERVICE;
  }

  /**
   * @brief Locate a DLNAServiceInfo by its stored SID.
   *
   * The method searches the currently selected device first (if set) and
   * then across the injected device list. If no matching SID is found a
   * reference to the internal sentinel @c NO_SERVICE is returned.
   *
   * @param sid Null-terminated subscription identifier to lookup.
   * @return Reference to the matching DLNAServiceInfo or @c NO_SERVICE when
   *         not found. Do NOT take ownership of the returned reference.
   */

  void attachHttpServer(HttpServer& server) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "DLNAControlPointMgr::attachHttpServer");
    p_http_server = &server;
    // register handler at the local path. If local_url is not set we use
    // a default path "/dlna/events"
    const char* path = "/dlna/events";
    if (p_local_url && !StrView(p_local_url->url()).isEmpty())
      path = p_local_url->path();

    void* ctx[1];
    ctx[0] = this;
    server.on(path, T_NOTIFY, notifyHandler, ctx, 1);
  }

  /**
   * @brief Convenience to subscribe or unsubscribe all services of the
   *        currently selected device.
   *
   * This is a convenience wrapper: it iterates the services of the
   * currently selected @c p_device and calls the per-service
   * @c subscribeToService()/@c unsubscribeFromService() methods.
   *
   * @param subscribe true to subscribe, false to unsubscribe.
   * @return true when all attempted operations returned success; false if
   *         at least one service operation failed or no device is selected.
   */
  bool subscribeNotifications(bool subscribe = true) {
    if (p_device == nullptr) return false;
    if (subscribe) return subscribeToDevice(*p_device);
    return unsubscribeFromDevice(*p_device);
  }
  /**
   * @brief Subscribe to all services exposed by @p device.
   *
   * Iterates over device.getServices() and calls @c subscribeToService() for
   * each service. Subscription metadata is stored on the corresponding
   * DLNAServiceInfo entry.
   *
   * @param device Reference to the device whose services will be subscribed.
   * @return true if all subscriptions succeeded; false otherwise (partial
   *         failures result in continued attempts for remaining services).
   */
  bool subscribeToDevice(DLNADeviceInfo& device) {
    bool ok = true;
    for (auto& service : device.getServices()) {
      if (!subscribeToService(service)) {
        DlnaLogger.log(DlnaLogLevel::Error, "Subscription to service %s failed",
                       service.service_id);
        ok = false;
      }
    }
    return ok;
  }

  /**
   * @brief Unsubscribe from all services of @p device.
   *
   * Iterates through the device's services and calls
   * @c unsubscribeFromService() for each. Errors are logged and the method
   * returns false if one or more unsubscriptions failed.
   *
   * @param device Device whose services should be unsubscribed.
   * @return true if all unsubscriptions succeeded; false otherwise.
   */
  bool unsubscribeFromDevice(DLNADeviceInfo& device) {
    bool ok = true;
    for (auto& service : device.getServices()) {
      if (!unsubscribeFromService(service)) {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "Unsubscribe from service %s failed",
                       service.service_id);
        ok = false;
      }
    }
    return ok;
  }

  /**
   * @brief Subscribe to a single eventing service described by @p service.
   *
   * Performs the SUBSCRIBE HTTP request using the injected @c DLNAHttpRequest
   * instance. On success the returned SID is stored in
   * @c DLNAServiceInfo::subscription_id and timing metadata is updated.
   *
   * Use @c event_subscription_duration_sec to control the TIMEOUT header.
   *
   * @param service Reference to the DLNAServiceInfo describing the eventing
   *                service to subscribe to. The method updates the service
   *                object in-place.
   * @return true when subscription succeeded and the SID was recorded.
   */
  bool subscribeToService(DLNAServiceInfo& service) {
    if (StrView(service.event_sub_url).isEmpty()) {
      return false;
    }
    Url url(service.event_sub_url);

    // If already subscribed and not expired, nothing to do
    if (service.subscription_state == SubscriptionState::Subscribed &&
        millis() < service.time_subscription_expires) {
      return true;
    }

    char seconds_txt[80] = {0};
    snprintf(seconds_txt, sizeof(seconds_txt), "Second-%d",
             event_subscription_duration_sec);
    p_http->request().put("NT", "upnp:event");
    p_http->request().put("TIMEOUT", seconds_txt);
    p_http->request().put("CALLBACK", p_local_url ? p_local_url->url() : "");
    // For re-subscribe, include existing SID header if present
    if (!service.subscription_id.isEmpty()) {
      p_http->request().put("SID", service.subscription_id.c_str());
    }

    int rc = p_http->subscribe(url);
    if (rc == 200) {
      const char* sid = p_http->reply().get("SID");
      if (sid != nullptr) {
        service.subscription_id = sid;
        service.subscription_state = SubscriptionState::Subscribed;
        service.time_subscription_started = millis();
        service.time_subscription_confirmed = millis();
        service.time_subscription_expires =
            millis() + (event_subscription_duration_sec * 1000);
        subscribe_timeout = service.time_subscription_expires;
        DlnaLogger.log(DlnaLogLevel::Info, "Subscribe %s -> rc=%d", url.url(),
                       rc);
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "Subscribe %s succeeded but no SID returned", url.url());
        return false;
      }
    } else {
      // failed to subscribe
      DlnaLogger.log(DlnaLogLevel::Error,
                     "Failed to subscribe to service %s, rc=%d",
                     service.service_id, rc);
      return false;
    }
    return true;
  }

  /**
   * @brief Unsubscribe from a single service.
   *
   * Issues an UNSUBSCRIBE request. On success the manager clears local SID
   * and timing metadata stored in @p service. If the remote call fails the
   * subscription_state is set to Unsubscribing and the error is returned.
   *
   * @param service Service information for the target eventing service.
   * @return true when the remote UNSUBSCRIBE returned HTTP 200 and local
   *         metadata was cleared; false otherwise.
   */
  bool unsubscribeFromService(DLNAServiceInfo& service) {
    if (StrView(service.event_sub_url).isEmpty()) {
      return false;
    }
    // If already subscribed and not expired, nothing to do
    if (service.subscription_state == SubscriptionState::Unsubscribed) {
      return true;
    }

    Url url(service.event_sub_url);
    int rc = p_http->unsubscribe(url, service.event_sub_sid);
    if (rc == 200) {
      DlnaLogger.log(DlnaLogLevel::Info, "Unsubscribe %s -> rc=%d", url.url(),
                     rc);
      // clear SID and metadata regardless of remote rc
      service.event_sub_sid = nullptr;
      service.subscription_id = "";
      service.subscription_state = SubscriptionState::Unsubscribed;
      service.time_subscription_confirmed = 0;
      service.time_subscription_expires = 0;
      subscribe_timeout = 0;
    } else {
      DlnaLogger.log(DlnaLogLevel::Error,
                     "Failed to unsubscribe from service %s, rc=%d",
                     service.service_id, rc);
      return false;
    }
    return true;
  }

  /**
   * @brief HTTP NOTIFY handler registered with @c HttpServer.
   *
   * This static method is registered as the endpoint that receives event
   * NOTIFY messages from remote UPnP services. It:
   *   - extracts the SID header from the request,
   *   - incrementally parses the message body using XMLParserPrint to extract
   *     property names and values, and
   *   - invokes the instance callback set via @c
   * setEventSubscriptionCallback().
   *
   * The handler accepts the server, request path and the handler context
   * line which embeds the pointer to the manager instance.
   *
   * @param server Pointer to the HttpServer handling the request.
   * @param requestPath Path of the incoming request (unused by
   * implementation).
   * @param handlerLine Pointer to the server-provided handler context which
   *                    contains the manager instance in its context array.
   */
  static void notifyHandler(HttpServer* server, const char* requestPath,
                            HttpRequestHandlerLine* handlerLine) {
    if (handlerLine == nullptr || handlerLine->contextCount < 1) return;
    void* ctx0 = handlerLine->context[0];
    SubscriptionMgrControlPoint* mgr =
        static_cast<SubscriptionMgrControlPoint*>(ctx0);
    if (!mgr) return;
    void* reference = mgr->event_callback_ref;

    // read SID header and body
    const char* sid = server->requestHeader().get("SID");

    // Use XMLParserPrint to incrementally parse the NOTIFY body and extract
    // property child elements. XMLParserPrint accumulates the buffer and
    // exposes a parse() method that returns the next fragment.

    // mark notify received early so subscription metadata gets updated
    mgr->updateReceived(sid);

    XMLParserPrint xml_parser;
    uint8_t buffer[XML_PARSER_BUFFER_SIZE];
    Str nodeName;
    Vector<Str> path;
    Str text;
    Str attrs;
    xml_parser.setExpandEncoded(true);
    Client& client = server->client();

    while (client.available()) {
      int read = client.read(buffer, XML_PARSER_BUFFER_SIZE);
      xml_parser.write(buffer, read);

      while (xml_parser.parse(nodeName, path, text, attrs)) {
        // We care about text nodes that are children of a <property> element
        if (text.isEmpty()) continue;

        // dispatch
        DLNAServiceInfo& s = mgr->getServiceInfoBySID(sid);
        if (mgr->event_callback) {
          const char* sid_c = sid ? sid : "";
          mgr->event_callback(sid_c, nodeName.c_str(), text.c_str(), reference);
        }
      }
    }

    // reply OK to the NOTIFY
    server->replyOK();
  }
};

}  // namespace tiny_dlna
