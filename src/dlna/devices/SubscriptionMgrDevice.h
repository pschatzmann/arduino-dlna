#pragma once

#include "basic/Logger.h"
#include "basic/NullPrint.h"
#include "basic/Str.h"
#include "basic/StrView.h"
#include "basic/Url.h"
#include "basic/Vector.h"
#include "dlna/StringRegistry.h"
#include "http/Http.h"
#include "http/Server/HttpRequest.h"

namespace tiny_dlna {

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
      nullptr;         /**< non-owning pointer to Subscription */
  Str changeNode;      /**< inner XML fragment to send */
  int error_count = 0; /**< number of failed send attempts */
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
 */
class SubscriptionMgrDevice {
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
  ~SubscriptionMgrDevice() {
    // free any remaining heap-allocated subscriptions
    for (int i = 0; i < subscriptions.size(); ++i) {
      delete subscriptions[i];
    }
    subscriptions.clear();
    // pending_list entries do not own subscription pointers
    pending_list.clear();
  }

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
   * @param sid Optional SID to renew; pass nullptr or empty to create a new subscription
   * @param timeoutSec Requested subscription timeout in seconds (default 1800)
   * @return Str Assigned or renewed SID for the subscription
   */
  Str subscribe(DLNAServiceInfo& service, const char* callbackUrl,
                const char* sid = nullptr, uint32_t timeoutSec = 1800) {
    // simple SID generation
    DlnaLogger.log(DlnaLogLevel::Info, "subscribe: %s %s",
                   StringRegistry::nullStr(service.service_id, "(null)"),
                   StringRegistry::nullStr(callbackUrl, "(null)"));
    // If sid provided, attempt to renew existing subscription for service
    if (sid != nullptr && !StrView(sid).isEmpty()) {
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
      // not found: fall through and create new subscription
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "uuid:%lu", millis());

    Subscription* s = new Subscription();
    s->sid = buffer;
    s->callback_url = callbackUrl;
    s->timeout_sec = timeoutSec;
    s->seq = 0;
    s->expires_at = millis() + (uint64_t)timeoutSec * 1000ULL;
    s->service = &service;

    subscriptions.push_back(s);
    return sid;
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
  bool unsubscribe(DLNAServiceInfo& service, const char* sid) {
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
   * Adds a pending notification for all subscribers of `service`. The
   * provided `xmlTag` is the inner XML fragment representing the changed
   * state (e.g. a LastChange Event fragment). Notifications are queued and
   * delivered later by calling `publish()`.
   *
   * @param service The DLNA service whose subscribers should be notified
   * @param xmlTag XML fragment describing the changed property
   */
  void addChange(DLNAServiceInfo& service, const char* xmlTag) {
    bool any = false;
    if (StrView(xmlTag).isEmpty()) {
      DlnaLogger.log(DlnaLogLevel::Warning, "empty xmlTag for service '%s'",
                     service.service_id);
      return;
    }
    // enqueue notifications to be posted later by post()
    for (int i = 0; i < subscriptions.size(); ++i) {
      Subscription* sub = subscriptions[i];
      if (sub->service != &service) continue;
      any = true;
      // increment seq now (reserve sequence number)
      sub->seq++;
      PendingNotification pn;
      // store pointer to the subscription entry (no snapshot)
      pn.p_subscription = subscriptions[i];
      pn.changeNode = xmlTag;
      pending_list.push_back(pn);
    }
    if (!any) {
      DlnaLogger.log(DlnaLogLevel::Error, "service '%s' not found",
                     service.service_id);
    }
  }

  /**
   * @brief Deliver queued notifications
   *
   * Iterates the pending notification queue and attempts to deliver each
   * notification via HTTP NOTIFY. Successful deliveries are removed from
   * the queue. Failed attempts increment an error counter; entries that
   * exceed MAX_SEND_ERRORS are dropped. Expired subscriptions are removed
   * prior to delivery.
   */
  void publish() {
    // First remove expired subscriptions so we don't deliver to them.
    removeExpired();
    if (pending_list.empty()) return;
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

      Subscription& sub = *pn.p_subscription;

      // Build and send HTTP notify as in previous implementation
      Url cbUrl(sub.callback_url.c_str());
      DLNAHttpRequest http;
      http.setHost(cbUrl.host());
      http.setAgent("tiny-dlna-notify");
      http.request().put("NT", "upnp:event");
      http.request().put("NTS", "upnp:propchange");
      char seqBuf[32];
      snprintf(seqBuf, sizeof(seqBuf), "%d", sub.seq);
      http.request().put("SEQ", seqBuf);
      http.request().put("SID", sub.sid.c_str());

      // compute xml length
      NullPrint np;
      size_t xmlLen = 0;
      const char* serviceAbbrev =
          sub.service ? sub.service->subscription_namespace_abbrev : "";
      if (is_log_xml)
        xmlLen = createXML(Serial, serviceAbbrev, pn.changeNode.c_str());
      else
        xmlLen = createXML(np, serviceAbbrev, pn.changeNode.c_str());

      auto printXML = [this, serviceAbbrev, pn](Print& out) {
        createXML(out, serviceAbbrev, pn.changeNode.c_str());
      };

      // post the notification
      http.setTimeout(DLNA_HTTP_REQUEST_TIMEOUT_MS);
      int rc = http.notify(cbUrl, xmlLen, printXML, "text/xml");
      DlnaLogger.log(DlnaLogLevel::Info, "Notify %s -> %d", cbUrl.url(), rc);

      // remove processed notification on success; otherwise keep it for retry
      if (rc == 200) {
        pending_list.erase(i);
        processed++;
        // do not increment i: elements shifted down into current index
      } else {
        // Increment error count on the stored entry (not the local copy)
        pending_list[i].error_count++;
        if (pending_list[i].error_count > MAX_SEND_ERRORS) {
          // give up and drop the entry after too many failed attempts
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "dropping notify to %s after %d errors with rc=%d %s", cbUrl.url(),
                         pending_list[i].error_count, rc, http.reply().statusMessage());
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
  }

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
   * @brief Number of active subscriptions
   * @return int count of subscriptions
   */
  int subscriptionsCount() { return subscriptions.size(); }

  /**
   * @brief Number of queued pending notifications
   * @return int count of pending notifications
   */
  int pendingCount() { return pending_list.size(); }

 protected:
  // store pointers to heap-allocated Subscription objects. This keeps
  // references stable (we manage lifetimes explicitly) and avoids
  // large copies when enqueuing notifications.
  Vector<Subscription*> subscriptions;
  Vector<PendingNotification> pending_list;
  bool is_log_xml = true;
  // Maximum send attempts before dropping a pending notification
  static constexpr int MAX_SEND_ERRORS = 3;

  /**
   * @brief Generate the propertyset XML for a single variable and write it to
   * the provided Print instance.
   *
   * Returns the number of bytes written.
   */
  size_t createXML(Print& out, const char* serviceAbbrev,
                   const char* changeNode) {
    size_t written = 0;
    written += out.print("<?xml version=\"1.0\"?>\r\n");
    written += out.print(
        "<e:propertyset "
        "xmlns:e=\"urn:schemas-upnp-org:metadata-1-0/events\">\r\n");
    written += out.print("<e:property>\r\n");
    written += out.print("<LastChange>\r\n");
    written += out.print("<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/");
    written += out.print(serviceAbbrev);
    written += out.print("/\">\r\n");
    written += out.print(changeNode);
    written += out.print("</Event>\r\n");
    written += out.print("</LastChange>\r\n");
    written += out.print("</e:property>\r\n");
    written += out.print("</e:propertyset>\r\n");
    return written;
  }
};

}  // namespace tiny_dlna
