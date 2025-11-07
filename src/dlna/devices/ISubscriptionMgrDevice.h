#pragma once

#include <functional>
#include <cstddef>
#include <cstdint>

#include "basic/Str.h"

class Print;

namespace tiny_dlna {

class DLNAServiceInfo;
class IHttpServer;

class ISubscriptionMgrDevice {
 public:
  virtual ~ISubscriptionMgrDevice() = default;

  virtual Str subscribe(DLNAServiceInfo& service, const char* callbackUrl,
                        const char* sid = nullptr,
                        uint32_t timeoutSec = 1800) = 0;

  virtual bool unsubscribe(DLNAServiceInfo& service, const char* sid) = 0;

  virtual void addChange(
      DLNAServiceInfo& service,
      std::function<size_t(Print&, void*)> changeWriter, void* ref) = 0;

  virtual void publish() = 0;

  virtual void removeExpired() = 0;

  virtual int subscriptionsCount() = 0;

  virtual int pendingCount() = 0;

  virtual void setSubscriptionsActive(bool flag) = 0;

  virtual void end() = 0;

  virtual bool isSubscriptionsActive() const = 0;

  virtual bool processSubscribeRequest(IHttpServer& server,
                                       DLNAServiceInfo& service) = 0;

  virtual bool processUnsubscribeRequest(IHttpServer& server,
                                         DLNAServiceInfo& service) = 0;
};

}  // namespace tiny_dlna
