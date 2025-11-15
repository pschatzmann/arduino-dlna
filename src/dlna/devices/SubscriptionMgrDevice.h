#pragma once

#include <functional>

#include "basic/EscapingPrint.h"
#include "basic/Logger.h"
#include "basic/NullPrint.h"
#include "basic/Str.h"
#include "basic/StrView.h"
#include "basic/Url.h"
#include "basic/Vector.h"
#include "http/Http.h"
#include "http/Server/HttpRequest.h"
#include "http/Server/IHttpServer.h"
#include "dlna/devices/ISubscriptionMgrDevice.h"

namespace tiny_dlna {

// forward declaration for friend
class DLNAService;
template <typename ClientType>
class DLNADevice;

/**
 * @struct Subscription
 * @brief Represents a single event subscription for a service.
 *
 * Holds the subscription identifier (SID), the subscriber's callback URL
 * (as provided in the CALLBACK header), the negotiated timeout (seconds),
 * a delivery sequence counter (`seq`) and an absolute expiration timestamp
 * (`expires_at`). Instances are stored by `SubscriptionMgr` per-service.
 */
struct Subscription {
  Str sid;
  Str callback_url;  // as provided in CALLBACK header
  uint32_t timeout_sec = 1800;
  uint32_t seq = 0;
  uint64_t expires_at = 0;
  DLNAServiceInfo* service = nullptr;
};
/**
 * @struct PendingNotification
 * @brief Representation of a queued notification to be delivered later.
 *
 * Holds a pointer to the subscription to notify and the XML change node
 * to send. `p_subscription` is a non-owning pointer into the `subscriptions`
 * container; code must handle the case where it becomes null (or the
 * subscription is removed via `unsubscribe`). `error_count` tracks delivery
 * failures and is used to drop entries after repeated errors.
 */
struct PendingNotification {
  Subscription* p_subscription =
      nullptr; /**< non-owning pointer to Subscription */
  std::function<size_t(Print&, void*)> writer; /**< writer callback that emits
                                              the notification XML */
  void* ref = nullptr; /**< opaque reference for writer */
  int error_count = 0; /**< number of failed send attempts */
  int seq = 0;
};

/**
 * @class SubscriptionMgrDevice
 * @brief Manages UPnP event subscriptions and queued notifications for a
 *        DLNA device.
 *
 * The manager stores active subscriptions (allocated on the heap) and a
 * queue of pending notifications that are delivered asynchronously via
 * HTTP NOTIFY requests. Subscriptions are represented by `Subscription`
 * objects; pending notifications reference subscriptions via a
 * non-owning pointer (`PendingNotification::p_subscription`).
 *
 * Responsibilities:
 * - create and remove subscriptions (SID generation)
 * - enqueue state-change notifications for delivery (`addChange`)
 * - deliver queued notifications (`publish`) with retry/drop policy
 * - purge expired subscriptions (`removeExpired`)
 *
 * Note: `p_subscription` pointers may become invalid if a subscription is
 * removed; removal routines attempt to clean up pending entries, but a
 * stable UID approach is recommended for maximum robustness.
 *
 * The template parameter defines the HTTP client type used when delivering
 * notifications (e.g. WiFiClient); callers should pass the client that is
 * compatible with their transport stack.
 *
 * @tparam ClientType Arduino `Client` implementation used for outgoing
 *         NOTIFY/SUBSCRIBE HTTP calls (e.g. `WiFiClient`, `EthernetClient`).
 */
template <typename ClientType>
class SubscriptionMgrDevice : public ISubscriptionMgrDevice {
    template <typename>
    friend class DLNADevice;

 public:
  /**
   * @brief Construct a new Subscription Manager
   *
   * Initializes an empty manager. Subscriptions are owned by the manager
   * (heap-allocated) and will be freed in the destructor.
   */
  SubscriptionMgrDevice() = default;

  /**
   * @brief Destroy the Subscription Manager
   *
   * Frees any remaining heap-allocated subscriptions and clears internal
   * pending lists. This ensures no memory is leaked when the manager is
   * destroyed.
   */
  ~SubscriptionMgrDevice() { clear(); }

  /**
   * @brief Add or renew a subscription for a service
   *
   * Creates a new subscription for the given service and callback URL, or
   * renews an existing one if a non-empty `sid` is provided and a matching
   * subscription exists for the service. When renewing, the existing SID is
   * returned and the timeout/expiry is updated. This mirrors UPnP semantics
   * where clients may SUBSCRIBE with an existing SID to renew.
   *
   * @param service Reference to the DLNA service to subscribe to
   * @param callbackUrl The CALLBACK URL provided by the subscriber. May be
   *        empty when renewing via SID.
   * @param sid Optional SID to renew; pass nullptr or empty to create a new
   * subscription
   * @param timeoutSec Requested subscription timeout in seconds (default 1800)
   * @return Str Assigned or renewed SID for the subscription
   */
  Str subscribe(DLNAServiceInfo& service, const char* callbackUrl,
                const char* sid = nullptr,
                uint32_t timeoutSec = 1800) override {
    // simple SID generation
    DlnaLogger.log(DlnaLogLevel::Info, "subscribe: %s %s",
                   StrView(service.service_id).c_str(),
                   StrView(callbackUrl).c_str());

    bool hasSid = !StrView(sid).isEmpty();

    // NEW subscription without a callback is invalid (GENA requirement)
    if (!hasSid) {
      if (StrView(callbackUrl).isEmpty()) {
        DlnaLogger.log(
            DlnaLogLevel::Warning,
            "subscribe: missing CALLBACK header for new subscription");
        return Str();
      }
    }

    // If sid provided, attempt to renew existing subscription for service
    if (hasSid) {
      Str renewed = renewSubscription(service, sid, callbackUrl, timeoutSec);
      if (!renewed.isEmpty()) return renewed;
      // not found: fall through and create new subscription
    }

    // generate uuid-based SID
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "uuid:%lu", millis());

    // fill subscription
    Subscription* s = new Subscription();
    s->sid = buffer;
    s->callback_url = callbackUrl;
    s->timeout_sec = timeoutSec;
    s->seq = 0;
    s->expires_at = millis() + (uint64_t)timeoutSec * 1000ULL;
    s->service = &service;

    for (auto& existing_sub : subscriptions) {
      if (existing_sub->service == &service &&
          StrView(existing_sub->callback_url).equals(callbackUrl)) {
        // Found existing subscription for same service and callback URL
        DlnaLogger.log(DlnaLogLevel::Info,
                       "subscribe: found existing subscription for service "
                       "'%s' and callback '%s', renewing SID '%s'",
                       StrView(service.service_id).c_str(),
                       StrView(callbackUrl).c_str(),
                       existing_sub->sid.c_str());
        // Renew existing subscription
        existing_sub->timeout_sec = timeoutSec;
        existing_sub->expires_at = millis() + (uint64_t)timeoutSec * 1000ULL;
        return existing_sub->sid;
      }
    }

    // add subscription
    subscriptions.push_back(s);
    return s->sid;
  }

  /**
   * @brief Remove a subscription identified by SID
   *
   * Removes the subscription for the specified service and SID. Pending
   * notifications targeting the subscription are also removed. The
   * subscription object is deleted and the method returns true on success.
   *
   * @param service Reference to the DLNA service
   * @param sid Subscription identifier to remove
   * @return true if the subscription was found and removed
   * @return false if no matching subscription exists
   */
  bool unsubscribe(DLNAServiceInfo& service, const char* sid) override {
    for (size_t i = 0; i < subscriptions.size(); ++i) {
      Subscription* s = subscriptions[i];
      if (s->service == &service && StrView(s->sid).equals(sid)) {
        // remove pending notifications that reference this subscription
        for (int j = 0; j < pending_list.size();) {
          if (pending_list[j].p_subscription == s) {
            pending_list.erase(j);
          } else {
            ++j;
          }
        }
        delete s;
        subscriptions.erase(i);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Enqueue a state-variable change for delivery
   *
   * Adds a pending notification for all subscribers of `service`.
   * Instead of passing a raw XML fragment, callers provide a small
   * writer callback that emits the inner XML fragment when invoked. The
   * manager will call this callback to compute the Content-Length (using
   * a `NullPrint`) and later again when streaming the NOTIFY body. The
   * opaque `ref` pointer is forwarded to the writer both during length
   * calculation and at publish time.
   *
   * @param service The DLNA service whose subscribers should be notified
   * @param changeWriter Callback that writes the inner XML fragment. It
   *        must match the signature `size_t(Print&, void*)` and return the
   *        number of bytes written.
   * @param ref Opaque pointer passed to `changeWriter` when it is invoked
   */
  void addChange(DLNAServiceInfo& service,
                 std::function<size_t(Print&, void*)> changeWriter,
                 void* ref) override {
    bool any = false;
    // do not enqueue if subscriptions are inactive
    if (!is_active) return;

    // enqueue notifications to be posted later by post()
    for (int i = 0; i < subscriptions.size(); ++i) {
      Subscription* sub = subscriptions[i];
      if (sub->service != &service) continue;
      any = true;
      NullPrint np;
      PendingNotification pn;
      // store pointer to the subscription entry (no snapshot)
      pn.p_subscription = subscriptions[i];
      pn.ref = ref;
      pn.writer = changeWriter;
      pn.seq = sub->seq;
      pending_list.push_back(pn);
      // increment seq AFTER queuing (so first notification uses SEQ=0)
      sub->seq++;
    }
    if (!any) {
      DlnaLogger.log(DlnaLogLevel::Info, "service '%s' has no subscriptions",
                     service.service_id.c_str());
    }
  }

  /**
   * @brief Deliver queued notifications
   *
   * Iterates the pending notification queue and attempts to deliver each
   * notification via HTTP NOTIFY. Successful deliveries are removed from
   * the queue. Failed attempts increment an error counter; entries that
   * exceed MAX_NOTIFY_RETIES are dropped. Expired subscriptions are removed
   * prior to delivery.
   */
  int publish() override {
    if (!is_active) {
      // clear any queued notifications when subscriptions are turned off
      clear();
      return 0;
    }
    // First remove expired subscriptions so we don't deliver to them.
    removeExpired();
    
    if (pending_list.empty()) return 0;
    // Attempt to process each pending notification once. If processing
    // fails (non-200), leave the entry in the queue for a later retry.
    int processed = 0;
    for (int i = 0; i < pending_list.size();) {
      // copy entry to avoid referencing vector storage while potentially
      // modifying the vector (erase)
      PendingNotification pn = pending_list[i];

      // Ensure the subscription pointer is valid; if not, drop the entry.
      if (pn.p_subscription == nullptr) {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "pending notification dropped: missing subscription");
        pending_list.erase(i);
        continue;
      }
      // determine subscription details
      Subscription& sub = *pn.p_subscription;

      // convert the sequence number to string
      char seqBuf[32];
      snprintf(seqBuf, sizeof(seqBuf), "%d", pn.seq);

      // Build and send HTTP notify as in previous implementation
      Url cbUrl(sub.callback_url.c_str());
      HttpRequest<ClientType> http;
      http.setTimeout(DLNA_HTTP_REQUEST_TIMEOUT_MS);
      http.setHost(cbUrl.host());
      http.setAgent("tiny-dlna-notify");
      http.request().put("NT", "upnp:event");
      http.request().put("NTS", "upnp:propchange");
      http.request().put("SEQ", seqBuf);
      http.request().put("SID", sub.sid.c_str());
      DlnaLogger.log(DlnaLogLevel::Info, "Notify %s", cbUrl.url());
      DlnaLogger.log(DlnaLogLevel::Info, "- SEQ: %s", seqBuf);
      DlnaLogger.log(DlnaLogLevel::Info, "- SID: %s", sub.sid.c_str());
      int rc = http.notify(cbUrl, createXML, "text/xml", &pn);

      // log result
      DlnaLogger.log(DlnaLogLevel::Info, "Notify %s -> %d", cbUrl.url(), rc);

      // remove processed notification on success; otherwise keep it for retry
      if (rc == 200) {
        pending_list.erase(i);
        processed++;
        // do not increment i: elements shifted down into current index
      } else {
        // Increment error count on the stored entry (not the local copy)
        pending_list[i].error_count++;
        if (pending_list[i].error_count > MAX_NOTIFY_RETIES) {
          // give up and drop the entry after too many failed attempts
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "dropping notify to %s after %d errors with rc=%d %s",
                         cbUrl.url(), pending_list[i].error_count, rc,
                         http.reply().statusMessage());
          pending_list.erase(i);
        } else {
          ++i;
        }
      }
    }

    DlnaLogger.log(
        DlnaLogLevel::Info,
        "Published: %d notifications, %d remaining (for %d subscriptions)",
        processed, pendingCount(), subscriptionsCount());
    return processed;
  }


  /**
   * @brief Number of active subscriptions
   * @return int count of subscriptions
   */
  size_t subscriptionsCount() override { return subscriptions.size(); }

  /**
   * @brief Number of queued pending notifications
   * @return int count of pending notifications
   */
  size_t pendingCount() override { return pending_list.size(); }

  /**
   * @brief Enable or disable subscription delivery.
   *
   * When disabled the manager will not attempt to publish pending
   * notifications. Disabling also clears the pending queue to avoid
   * delivering stale notifications when re-enabled. Use
   * `isSubscriptionsActive()` to query the current state.
   */
  void setSubscriptionsActive(bool flag) override {
    is_active = flag;
  }

  /// Convenience method to disable subscriptions at the end of the lifecycle
  void end() override { setSubscriptionsActive(false); }

  /**
   * @brief Query whether subscription delivery is active
   * @return true if subscription delivery is enabled
   */
  bool isSubscriptionsActive() override { return is_active; }


 protected:
  // store pointers to heap-allocated Subscription objects. This keeps
  // references stable (we manage lifetimes explicitly) and avoids
  // large copies when enqueuing notifications.
  Vector<Subscription*> subscriptions;
  Vector<PendingNotification> pending_list;
  bool is_active = true;

  /**
   * @brief Remove expired subscriptions
   *
   * Walks the subscription list and unsubscribes any entry whose
   * `expires_at` timestamp has passed. This also removes pending
   * notifications related to the unsubscribed entries.
   */
  void removeExpired() {
    uint64_t now = millis();
    for (int i = 0; i < subscriptions.size();) {
      Subscription* s = subscriptions[i];
      if (s->expires_at != 0 && s->expires_at <= now) {
        DlnaLogger.log(DlnaLogLevel::Info, "removing expired subscription %s",
                       s->sid.c_str());
        // Use unsubscribe() which will remove pending notifications and
        // free the subscription object. Do not increment `i` because the
        // erase shifts the vector content down into the same index.
        unsubscribe(*s->service, s->sid.c_str());
      } else {
        ++i;
      }
    }
  }

  /**
   * @brief Try to renew an existing subscription identified by SID for the
   * given service. If successful the subscription's timeout/expiry (and
   * optionally callback URL) are updated and the renewed SID is returned.
   * If no matching subscription exists an empty Str is returned.
   */
  Str renewSubscription(DLNAServiceInfo& service, const char* sid,
                        const char* callbackUrl, uint32_t timeoutSec) {
    if (StrView(sid).isEmpty()) return Str();
    for (int i = 0; i < subscriptions.size(); ++i) {
      Subscription* ex = subscriptions[i];
      if (ex->service == &service && StrView(ex->sid).equals(sid)) {
        // renew: update timeout and expiry, do not change SID
        ex->timeout_sec = timeoutSec;
        ex->expires_at = millis() + (uint64_t)timeoutSec * 1000ULL;
        // if a new callback URL was provided, update it
        if (callbackUrl != nullptr && !StrView(callbackUrl).isEmpty()) {
          ex->callback_url = callbackUrl;
        }
        DlnaLogger.log(DlnaLogLevel::Info, "renewed subscription %s",
                       ex->sid.c_str());
        return ex->sid;
      }
    }
    return Str();
  }

  /// Clear all subscriptions and pending notifications
  void clear() {
    // free any remaining heap-allocated subscriptions
    for (int i = 0; i < subscriptions.size(); ++i) {
      delete subscriptions[i];
    }
    subscriptions.clear();
    // pending_list entries do not own subscription pointers
    pending_list.clear();
  }

  /**
   * @brief Generate the propertyset XML for a single variable and write it to
   * the provided Print instance. The inner event content is emitted via the
   * provided `changeWriter` callback which must have signature
   * `size_t(Print&, void*)`. The opaque `ref` pointer is passed through to
   * the callback and may be used to access context (for example the
   * `PendingNotification` that owns the fragment).
   *
   * Returns the number of bytes written.
   */
  static size_t createXML(Print& out, void* ref) {
    EscapingPrint out_esc{out};
    PendingNotification& pn = *(PendingNotification*)ref;
    const char* service_abbrev =
        pn.p_subscription->service->subscription_namespace_abbrev;
    int instance_id = pn.p_subscription->service->instance_id;

    size_t written = 0;
    written += out.println("<?xml version=\"1.0\"?>");
    written += out.println(
        "<e:propertyset "
        "xmlns:e=\"urn:schemas-upnp-org:metadata-1-0/events\">");
    written += out.println("<e:property>");
    written += out.println("<LastChange>");
    // blow should be escaped
    written +=
        out_esc.print("<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/");
    written += out_esc.print(service_abbrev);
    written += out_esc.println("/\">");
    written += out_esc.print("<InstanceID val=\"");
    written += out_esc.print(instance_id);
    written += out_esc.println("\">");
    if (pn.writer) {
      written += pn.writer(out_esc, pn.ref);
    }
    written += out_esc.println("</InstanceID>");
    written += out_esc.println("</Event>");
    // end of escaped
    written += out.println("</LastChange>");
    written += out.println("</e:property>");
    written += out.println("</e:propertyset>");
    return written;
  }

  /**
   * @brief Handle SUBSCRIBE request including HTTP response
   *
   * Parses the CALLBACK, TIMEOUT, and SID headers from the HTTP request,
   * processes the subscription, and sends the HTTP response back to the client.
   * This method encapsulates all HTTP-specific logic for SUBSCRIBE requests.
   *
   * @param server Reference to the HTTP server handling the request
   * @param service Reference to the DLNA service to subscribe to
   * @return true if subscription was successful and response sent
   */
  bool processSubscribeRequest(IHttpServer& server,
                               DLNAServiceInfo& service) override {
    // Get headers from request
    const char* callbackHeader = server.requestHeader().get("CALLBACK");
    const char* timeoutHeader = server.requestHeader().get("TIMEOUT");
    const char* sidHeader = server.requestHeader().get("SID");

    // Log the incoming headers
    DlnaLogger.log(DlnaLogLevel::Info, "- SUBSCRIBE CALLBACK: %s",
                   callbackHeader ? callbackHeader : "null");
    DlnaLogger.log(DlnaLogLevel::Info, "- SUBSCRIBE TIMEOUT: %s",
                   timeoutHeader ? timeoutHeader : "null");
    DlnaLogger.log(DlnaLogLevel::Info, "- SUBSCRIBE SID: %s",
                   sidHeader ? sidHeader : "null");

    // Parse and clean callback URL (remove angle brackets)
    Str cbStr;
    if (callbackHeader) {
      cbStr = callbackHeader;
      cbStr.replace("<", "");
      cbStr.replace(">", "");
      cbStr.trim();
    }

    // Sanitize SID (remove optional angle brackets)
    Str sidStr;
    if (sidHeader) {
      sidStr = sidHeader;
      sidStr.replace("<", "");
      sidStr.replace(">", "");
      sidStr.trim();
    }

    // Parse timeout (extract seconds from "Second-1800" format)
    uint32_t tsec = 1800;
    if (timeoutHeader && StrView(timeoutHeader).startsWith("Second-")) {
      tsec = atoi(timeoutHeader + 7);
    }

    // Create or renew subscription
    const char* callbackPtr = cbStr.isEmpty() ? nullptr : cbStr.c_str();
    const char* sidPtr = sidStr.isEmpty() ? nullptr : sidStr.c_str();

    Str sid = subscribe(service, callbackPtr, sidPtr, tsec);
    if (sid.isEmpty()) {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "subscribe request rejected (missing data)");
      server.replyHeader().setValues(412, "Precondition Failed");
      server.replyHeader().put("Content-Length", 0);
      server.replyHeader().write(server.client());
      server.endClient();
      return false;
    }
    DlnaLogger.log(DlnaLogLevel::Info, "- SID: %s", sid.c_str());

    // Send SUBSCRIBE response
    server.replyHeader().setValues(200, "OK");
    server.replyHeader().put("SID", sid.c_str());
    server.replyHeader().put("TIMEOUT", "Second-1800");
    server.replyHeader().put("Content-Length", 0);
    server.replyHeader().write(server.client());
    server.endClient();

    return true;
  }

  /**
   * @brief Handle UNSUBSCRIBE request including HTTP response
   *
   * Parses the SID header from the HTTP request, removes the subscription,
   * and sends the appropriate HTTP response back to the client.
   * This method encapsulates all HTTP-specific logic for UNSUBSCRIBE requests.
   *
   * @param server Reference to the HTTP server handling the request
   * @param service Reference to the DLNA service to unsubscribe from
   * @return true if unsubscribe was successful and response sent
   */
  bool processUnsubscribeRequest(IHttpServer& server,
                                 DLNAServiceInfo& service) override {
    const char* sid = server.requestHeader().get("SID");

    DlnaLogger.log(DlnaLogLevel::Info, "- UNSUBSCRIBE SID: %s",
                   sid ? sid : "null");

    if (sid) {
      bool ok = unsubscribe(service, sid);
      if (ok) {
        DlnaLogger.log(DlnaLogLevel::Info, "Unsubscribed: %s", sid);
        server.replyOK();
        return true;
      }
    }

    // SID not found or invalid
    DlnaLogger.log(DlnaLogLevel::Warning,
                   "handleUnsubscribeRequest: failed for SID %s",
                   sid ? sid : "(null)");
    server.replyNotFound();
    return false;
  }
};

}  // namespace tiny_dlna
