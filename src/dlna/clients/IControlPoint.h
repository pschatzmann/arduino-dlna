#pragma once

#include <stdint.h>

#include <functional>

#include "Client.h"
#include "basic/Url.h"
#include "dlna/Action.h"
#include "dlna/udp/IUDPService.h"
#include "http/Server/IHttpRequest.h"
#include "http/Server/IHttpServer.h"

namespace tiny_dlna {

// Forward declaration of templated Vector with two parameters matches
// implementation
template <class T, class Alloc>
class Vector;

class DLNADeviceInfo;
class DLNAServiceInfo;
class SubscriptionMgrControlPoint;

using XMLCallback = std::function<void(Client& client, ActionReply& reply)>;

class DLNADeviceInfo;
class DLNAServiceInfo;
class SubscriptionMgrControlPoint;

/**
 * @class IControlPoint
 * @brief Abstract interface for DLNA Control Point functionality
 *
 * Defines the contract for implementing a DLNA Control Point that discovers,
 * manages, and controls DLNA devices on the network. Provides methods for
 * device discovery, service interaction, action execution, and event
 * subscription.
 *
 * Implementations should handle SSDP discovery, SOAP action invocation,
 * and UPnP event subscription management.
 */
class IControlPoint {
 public:
  virtual ~IControlPoint() = default;

  /// Enable/disable parsing of device descriptions during discovery
  virtual void setParseDevice(bool flag) = 0;
  /// Set the local callback URL for event subscriptions
  virtual void setLocalURL(Url url) = 0;
  /// Set the local callback URL for event subscriptions
  virtual void setLocalURL(IPAddress url, int port = 9001,
                           const char* path = "") = 0;
  /// Set the repeat interval for M-SEARCH requests
  virtual void setSearchRepeatMs(int repeatMs) = 0;
  /// Set user reference pointer for callbacks
  virtual void setReference(void* ref) = 0;
  /// Set index of device to operate on
  virtual void setDeviceIndex(int idx) = 0;
  /// Set callback for event subscription notifications
  virtual void setEventSubscriptionCallback(
      std::function<void(const char* sid, const char* varName,
                         const char* newValue, void* reference)>
          cb,
      void* ref = nullptr) = 0;
  /// Set HTTP server for handling event callbacks
  virtual void setHttpServer(IHttpServer& server) = 0;
  /// Set callback for processing XML result nodes
  virtual void onResultNode(
      std::function<void(const char* nodeName, const char* text,
                         const char* attributes)>
          cb) = 0;
  /// Start discovery with default HTTP/UDP services
  virtual bool begin(const char* searchTarget = "ssdp:all",
                     uint32_t minWaitMs = 3000, uint32_t maxWaitMs = 60000) = 0;
  /// Start discovery with provided HTTP/UDP services
  virtual bool begin(IHttpRequest& http, IUDPService& udp,
                     const char* searchTarget = "ssdp:all",
                     uint32_t minWaitMs = 3000, uint32_t maxWaitMs = 60000) = 0;
  /// Stop discovery and cleanup resources
  virtual void end() = 0;
  /// Queue an action for execution
  virtual ActionRequest& addAction(ActionRequest act) = 0;
  /// Execute queued actions and process responses
  virtual ActionReply& executeActions(XMLCallback xmlProcessor = nullptr) = 0;
  /// Process discovery loop and handle events
  virtual bool loop() = 0;
  /// Get service by ID from discovered devices
  virtual DLNAServiceInfo& getService(const char* id) = 0;
  /// Get current device (by index)
  virtual DLNADeviceInfo& getDevice() = 0;
  /// Get device by index
  virtual DLNADeviceInfo& getDevice(int idx) = 0;
  /// Get device containing the specified service
  virtual DLNADeviceInfo& getDevice(DLNAServiceInfo& service) = 0;
  /// Get device by location URL
  virtual DLNADeviceInfo& getDevice(Url location) = 0;
  /// Get list of all discovered devices
  virtual Vector<DLNADeviceInfo>& getDevices() = 0;
  /// Build URL for device service endpoint
  virtual const char* getUrl(DLNADeviceInfo& device, const char* suffix,
                             char* buffer, int len) = 0;
  /// Manually add a device to the list
  virtual bool addDevice(DLNADeviceInfo dev) = 0;
  /// Add device by discovery URL
  virtual bool addDevice(Url url) = 0;
  /// Set active state for processing
  virtual void setActive(bool flag) = 0;
  /// Check if control point is active
  virtual bool isActive() = 0;
  /// Allow localhost devices in discovery
  virtual void setAllowLocalhost(bool flag) = 0;
  /// Get last action reply
  virtual ActionReply& getLastReply() = 0;
  /// Get subscription manager for event handling
  virtual SubscriptionMgrControlPoint* getSubscriptionMgr() = 0;
  /// Enable/disable event subscription notifications
  virtual void setSubscribeNotificationsActive(bool flag) = 0;
  /// Register string for memory management
  virtual const char* registerString(const char* s) = 0;
};

}  // namespace tiny_dlna
