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
  SubscriptionMgrDevice() {}

  // Add or renew subscription. callbackUrl is formatted like
  // "<http://host:port/path>" Returns SID (generated if new)
  Str subscribe(const char* serviceId, const char* callbackUrl,
                uint32_t timeoutSec = 1800) {
    // simple SID generation
    DlnaLogger.log(DlnaLogLevel::Info, "subscribe: %s %s",
                   StringRegistry::nullStr(serviceId, "(null)"),
                   StringRegistry::nullStr(callbackUrl, "(null)"));
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "uuid:%lu", millis());
    Str sid = buffer;

    Subscription s;
    s.sid = sid;
    s.callback_url = callbackUrl;
    s.timeout_sec = timeoutSec;
    s.seq = 0;
    s.expires_at = millis() + (uint64_t)timeoutSec * 1000ULL;

    getList(serviceId).push_back(s);
    return sid;
  }

  bool unsubscribe(const char* serviceId, const char* sid) {
    auto& list = getList(serviceId);
    for (size_t i = 0; i < list.size(); ++i) {
      if (StrView(list[i].sid).equals(sid)) {
        list.erase(i);
        return true;
      }
    }
    return false;
  }

  // Publish a single state variable change to all subscribers of the service
  void publishProperty(const char* serviceId, const char* varName,
                       const char* value) {
    auto& list = getList(serviceId);
    for (int i = 0; i < list.size(); ++i) {
      Subscription& sub = list[i];
      // increment seq
      sub.seq++;
      // build and stream body via createXML

      // parse callback url
      Url cbUrl(sub.callback_url.c_str());
      DLNAHttpRequest http;
      http.setHost(cbUrl.host());
      http.setAgent("tiny-dlna-notify");
      http.request().put("NT", "upnp:event");
      http.request().put("NTS", "upnp:propchange");
      // SEQ header name in some stacks is "SEQ" or "SID"; use "SEQ" here
      char seqBuf[32];
      snprintf(seqBuf, sizeof(seqBuf), "%d", sub.seq);
      http.request().put("SEQ", seqBuf);
      http.request().put("SID", sub.sid.c_str());
      // post the notification using dynamic XML generation (streamed writer)
      // compute length by writing to NullPrint and then stream via lambda
      NullPrint np;
      size_t xmlLen = createXML(np, varName, value);
      auto printXML = [this, varName, value](Print& out) {
        createXML(out, varName, value);
      };
      int rc = http.post(cbUrl, xmlLen, printXML, "text/xml");
      DlnaLogger.log(DlnaLogLevel::Info, "Notify %s -> %d", cbUrl.url(), rc);
    }
  }

 protected:
  // naive per-service storage using parallel vectors
  Vector<Str> service_names;
  Vector<Vector<Subscription>> service_lists;

  Vector<Subscription>& getList(const char* serviceId) {
    for (int i = 0; i < service_names.size(); ++i) {
      if (StrView(service_names[i]).equals(serviceId)) return service_lists[i];
    }
    // not found: create
    Str s = serviceId;
    Vector<Subscription> list;
    service_names.push_back(s);
    service_lists.push_back(list);
    return service_lists[service_lists.size() - 1];
  }

  /**
   * @brief Generate the propertyset XML for a single variable and write it to
   * the provided Print instance.
   *
   * Returns the number of bytes written.
   */
  size_t createXML(Print& out, const char* varName, const char* value) {
    size_t written = 0;
    written += out.print("<?xml version=\"1.0\"?>\r\n");
    written += out.print(
        "<e:propertyset "
        "xmlns:e=\"urn:schemas-upnp-org:metadata-1-0/events\">\r\n");
    written += out.print("  <e:property>\r\n");
    written += out.print("    <");
    written += out.print(varName);
    written += out.print(">");
    written += out.print(value);
    written += out.print("</");
    written += out.print(varName);
    written += out.print(">\r\n");
    written += out.print("  </e:property>\r\n");
    written += out.print("</e:propertyset>\r\n");
    return written;
  }
};

}  // namespace tiny_dlna
