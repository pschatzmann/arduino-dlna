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
 * Pending notification to be posted later by post()
 */
struct PendingNotification {
  // Pointer to the original subscription entry at enqueue time. We keep
  // a pointer only (no copy) to reduce memory overhead. Note: this may
  // become invalid if the subscriptions vector is reallocated/modified,
  // callers must handle null/dangling pointers conservatively.
  Subscription* p_subscription = nullptr;
  Str changeNode;
  int error_count = 0;
};

/**
 * @class SubscriptionMgr
 * @brief Manages event subscriptions and notification delivery.
 *
 * The manager maintains per-service lists of `Subscription` entries. It
 * provides methods to subscribe/unsubscribe clients and to publish state
 * variable changes (which will be delivered as UPnP event NOTIFY requests
 * to each subscriber's callback URL).
 *
 * Notes:
 * - Subscriptions are tracked in simple parallel vectors (service names and
 *   lists) rather than a hash map for minimal embedded footprint.
 * - `subscribe()` returns the assigned SID (generated when creating a new
 *   subscription). `unsubscribe()` removes a subscription by SID.
 */
class SubscriptionMgrDevice {
 public:
  SubscriptionMgrDevice() = default;
  ~SubscriptionMgrDevice() {
    // free any remaining heap-allocated subscriptions
    for (int i = 0; i < subscriptions.size(); ++i) {
      delete subscriptions[i];
    }
    subscriptions.clear();
    // pending_list entries do not own subscription pointers
    pending_list.clear();
  }

  // Add or renew subscription. callbackUrl is formatted like
  // "<http://host:port/path>" Returns SID (generated if new)
  Str subscribe(DLNAServiceInfo& service, const char* callbackUrl,
                uint32_t timeoutSec = 1800) {
    // simple SID generation
    DlnaLogger.log(DlnaLogLevel::Info, "subscribe: %s %s",
                   StringRegistry::nullStr(service.service_id, "(null)"),
                   StringRegistry::nullStr(callbackUrl, "(null)"));
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "uuid:%lu", millis());
    Str sid = buffer;

    Subscription* s = new Subscription();
    s->sid = sid;
    s->callback_url = callbackUrl;
    s->timeout_sec = timeoutSec;
    s->seq = 0;
    s->expires_at = millis() + (uint64_t)timeoutSec * 1000ULL;
    s->service = &service;

    subscriptions.push_back(s);
    return sid;
  }

  /// Remove the subscription
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

  /// Publish a single state variable change to all subscribers of the service
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

  /// Publish all queued notifications. This will attempt to deliver each
  /// pending notification and remove it from the queue.
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
      int rc = http.post(cbUrl, xmlLen, printXML, "text/xml");
      DlnaLogger.log(DlnaLogLevel::Info, "Notify %s -> %d", cbUrl.url(), rc);

      // remove processed notification on success; otherwise keep it for retry
      if (rc == 200) {
        pending_list.erase(i);
        processed++;
        // do not increment i: elements shifted down into current index
      } else {
        // Increment error count on the stored entry (not the local copy)
        // increment the stored entry's error count
        pending_list[i].error_count++;
        if (pending_list[i].error_count > MAX_SEND_ERRORS) {
          // give up and drop the entry after too many failed attempts
          DlnaLogger.log(DlnaLogLevel::Warning,
                         "dropping notify to %s after %d errors", cbUrl.url(),
                         pending_list[i].error_count);
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

  /// Remove subscriptions that have expired (expires_at <= now).
  /// Also removes any pending notifications that belonged to the removed
  /// subscriptions so we don't attempt to deliver to unsubscribed callbacks.
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

  int subscriptionsCount() { return subscriptions.size(); }

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
