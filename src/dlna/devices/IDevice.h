#pragma once

#include <functional>

class Print;

namespace tiny_dlna {

class DLNADeviceInfo;
class ISubscriptionMgrDevice;
class DLNAServiceInfo;
class IUDPService;
class IHttpServer;

class IHttpServer;

/// Loop Action flag
enum LoopAction { RUN_SERVER = 1, RUN_UDP = 2, RUN_SUBSCRIPTIONS = 4, RUN_ALL = 7 };


/**
 * @class IDevice
 * @brief Abstract interface for DLNA device functionality
 *
 * Defines the contract for implementing a DLNA device that can advertise itself
 * on the network, handle service requests, manage event subscriptions, and
 * provide UPnP device capabilities. Provides methods for device lifecycle,
 * service management, event handling, and periodic announcements.
 */
class IDevice {
 public:
  virtual ~IDevice() = default;

  /// Initialize device with device info, UDP service, and HTTP server
  virtual bool begin(DLNADeviceInfo& device, IUDPService& udp,
                     IHttpServer& server) = 0;
  /// Get subscription manager for event handling
  virtual ISubscriptionMgrDevice& getSubscriptionMgr() = 0;
  /// Stop device and cleanup resources
  virtual void end() = 0;
  /// Process device loop for UDP and scheduler operations
  virtual bool loop(int loopActions = RUN_ALL) = 0;
  /// Process HTTP server loop
  virtual bool loopServer() = 0;
  /// Get service by ID
  virtual DLNAServiceInfo& getService(const char* id) = 0;
  /// Get service by abbreviation
  virtual DLNAServiceInfo& getServiceByAbbrev(const char* abbrev) = 0;
  /// Get service by event subscription path
  virtual DLNAServiceInfo* getServiceByEventPath(const char* requestPath) = 0;
  /// Add state change event for notification
  virtual void addChange(const char* serviceAbbrev,
                         std::function<size_t(Print&, void*)> changeWriter,
                         void* ref) = 0;
  /// Get device information
  virtual DLNADeviceInfo& getDeviceInfo() = 0;
  /// Enable/disable scheduler for periodic announcements
  virtual void setSchedulerActive(bool flag) = 0;
  /// Check if scheduler is active
  virtual bool isSchedulerActive() = 0;
  /// Set repeat interval for alive announcements (ms)
  virtual void setPostAliveRepeatMs(uint32_t ms) = 0;
  /// Enable/disable event subscriptions
  virtual void setSubscriptionsActive(bool flag) = 0;
  /// Check if subscriptions are active
  virtual bool isSubscriptionsActive() const = 0;
  /// Set user reference pointer
  virtual void setReference(void* ref) = 0;
  /// Get user reference pointer
  virtual void* getReference() = 0;
};

}  // namespace tiny_dlna
