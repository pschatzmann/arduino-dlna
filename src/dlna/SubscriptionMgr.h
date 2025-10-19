#pragma once

#include "basic/Str.h"
#include "basic/Vector.h"
#include "basic/StrPrint.h"
#include "basic/Url.h"
#include "http/HttpServer.h"
#include "http/Server/HttpRequest.h"

namespace tiny_dlna {

// Minimal subscription manager: stores subscriptions per service and allows
// publishing of simple property change notifications (evented variables).

struct Subscription {
  Str sid;
  Str callback_url; // as provided in CALLBACK header
  uint32_t timeout_sec = 1800;
  uint32_t seq = 0;
  uint64_t expires_at = 0;
};

class SubscriptionMgr {
 public:
  SubscriptionMgr() {}

  // Add or renew subscription. callbackUrl is formatted like "<http://host:port/path>"
  // Returns SID (generated if new)
  Str subscribe(const char* serviceId, const char* callbackUrl, uint32_t timeoutSec = 1800) {
    // simple SID generation
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
    auto &list = getList(serviceId);
    for (size_t i = 0; i < list.size(); ++i) {
      if (StrView(list[i].sid).equals(sid)) {
        list.erase(i);
        return true;
      }
    }
    return false;
  }

  // Publish a single state variable change to all subscribers of the service
  void publishProperty(const char* serviceId, const char* varName, const char* value) {
    auto &list = getList(serviceId);
    for (int i = 0; i < list.size(); ++i) {
      Subscription &sub = list[i];
      // increment seq
      sub.seq++;
      // build body
      StrPrint body;
      body.reset();
      body.write((const uint8_t*)"<?xml version=\"1.0\"?>\r\n", 27);
      body.write((const uint8_t*)"<e:propertyset xmlns:e=\"urn:schemas-upnp-org:metadata-1-0/events\">\r\n", 68);
      body.write((const uint8_t*)"  <e:property>\r\n", 17);
      body.write((const uint8_t*)"    <", 6);
      body.write((const uint8_t*)varName, strlen(varName));
      body.write((const uint8_t*)">", 1);
      body.write((const uint8_t*)value, strlen(value));
      body.write((const uint8_t*)"</", 2);
      body.write((const uint8_t*)varName, strlen(varName));
      body.write((const uint8_t*)">\r\n", 3);
      body.write((const uint8_t*)"  </e:property>\r\n", 18);
      body.write((const uint8_t*)"</e:propertyset>\r\n", 21);

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
      // post the notification
      int rc = http.post(cbUrl, "text/xml", body.c_str(), body.length());
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
};

} // namespace tiny_dlna
