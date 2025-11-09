#pragma once

#include <functional>
#include <cstddef>
#include <cstdint>

#include "basic/Str.h"

class Print;

namespace tiny_dlna {

class DLNAServiceInfo;
class IHttpServer;

class IHttpServer;

/**
 * @class ISubscriptionMgrDevice
 * @brief Abstract interface for UPnP event subscription management
 *
 * Defines the contract for managing UPnP event subscriptions on DLNA devices.
 * Handles subscription lifecycle, event notification publishing, and HTTP
 * request processing for SUBSCRIBE/UNSUBSCRIBE operations. Manages event
 * queues and ensures timely delivery of state change notifications.
 */
class ISubscriptionMgrDevice {
 public:
  virtual ~ISubscriptionMgrDevice() = default;

  /// Subscribe to service events with callback URL
  virtual Str subscribe(DLNAServiceInfo& service, const char* callbackUrl,
                        const char* sid = nullptr,
                        uint32_t timeoutSec = 1800) = 0;

  /// Unsubscribe from service events
  virtual bool unsubscribe(DLNAServiceInfo& service, const char* sid) = 0;

  /// Add state change event for notification
  virtual void addChange(
      DLNAServiceInfo& service,
      std::function<size_t(Print&, void*)> changeWriter, void* ref) = 0;

  /// Publish pending event notifications
  virtual int publish() = 0;

  /// Remove expired subscriptions
  virtual void removeExpired() = 0;

  /// Provide actual number of subscriptions
  virtual size_t subscriptionsCount() = 0;

  /// Provide actual number of open (unprocessed) notifications
  virtual size_t pendingCount() = 0;

  /// Enable/disable subscription processing
  virtual void setSubscriptionsActive(bool flag) = 0;

  /// Cleanup and stop subscription manager
  virtual void end() = 0;

  /// Check if subscriptions are active
  virtual bool isSubscriptionsActive() = 0;

  /// Process HTTP SUBSCRIBE request
  virtual bool processSubscribeRequest(IHttpServer& server,
                                       DLNAServiceInfo& service) = 0;

  /// Process HTTP UNSUBSCRIBE request
  virtual bool processUnsubscribeRequest(IHttpServer& server,
                                         DLNAServiceInfo& service) = 0;
};

}  // namespace tiny_dlna
