#pragma once

#include <functional>

class Print;

namespace tiny_dlna {

class DLNADeviceInfo;
class ISubscriptionMgrDevice;
class DLNAServiceInfo;
class IUDPService;
class IHttpServer;

class IDevice {
 public:
  virtual ~IDevice() = default;

  virtual bool begin(DLNADeviceInfo& device, IUDPService& udp,
                     IHttpServer& server) = 0;
  virtual ISubscriptionMgrDevice& getSubscriptionMgr() = 0;
  virtual void end() = 0;
  virtual bool loop() = 0;
  virtual bool loopServer() = 0;
  virtual DLNAServiceInfo& getService(const char* id) = 0;
  virtual DLNAServiceInfo& getServiceByAbbrev(const char* abbrev) = 0;
  virtual DLNAServiceInfo* getServiceByEventPath(const char* requestPath) = 0;
  virtual void addChange(const char* serviceAbbrev,
                         std::function<size_t(Print&, void*)> changeWriter,
                         void* ref) = 0;
  virtual DLNADeviceInfo& getDeviceInfo() = 0;
  virtual void setSchedulerActive(bool flag) = 0;
  virtual bool isSchedulerActive() = 0;
  virtual void setPostAliveRepeatMs(uint32_t ms) = 0;
  virtual void setSubscriptionsActive(bool flag) = 0;
  virtual bool isSubscriptionsActive() const = 0;
  virtual void setReference(void* ref) = 0;
  virtual void* getReference() = 0;
};

}  // namespace tiny_dlna
