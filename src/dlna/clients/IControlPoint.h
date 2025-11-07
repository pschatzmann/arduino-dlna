#pragma once

#include <stdint.h>

#include <functional>

#include "Client.h"
#include "basic/Url.h"
#include "dlna/Action.h"
#include "http/Server/IHttpRequest.h"
#include "http/Server/IHttpServer.h"
#include "udp/IUDPService.h"
// #include "http/Server/HttpServerTypes.h"

namespace tiny_dlna {

template <class T>
class Vector;

class DLNADeviceInfo;
class DLNAServiceInfo;
class SubscriptionMgrControlPoint;

using XMLCallback = std::function<void(Client& client, ActionReply& reply)>;

class IControlPoint {
 public:
  virtual ~IControlPoint() = default;

  virtual void setParseDevice(bool flag) = 0;
  virtual void setLocalURL(Url url) = 0;
  virtual void setSearchRepeatMs(int repeatMs) = 0;
  virtual void setReference(void* ref) = 0;
  virtual void setDeviceIndex(int idx) = 0;
  virtual void setEventSubscriptionCallback(
      std::function<void(const char* sid, const char* varName,
                         const char* newValue, void* reference)>
          cb,
      void* ref = nullptr) = 0;
  virtual void setHttpServer(IHttpServer& server, int port = 80) = 0;
  virtual void onResultNode(
      std::function<void(const char* nodeName, const char* text,
                         const char* attributes)>
          cb) = 0;
  virtual bool begin(const char* searchTarget = "ssdp:all",
                     uint32_t minWaitMs = 3000, uint32_t maxWaitMs = 60000) = 0;
  virtual bool begin(IHttpRequest& http, IUDPService& udp,
                     const char* searchTarget = "ssdp:all",
                     uint32_t minWaitMs = 3000, uint32_t maxWaitMs = 60000) = 0;
  virtual void end() = 0;
  virtual ActionRequest& addAction(ActionRequest act) = 0;
  virtual ActionReply& executeActions(XMLCallback xmlProcessor = nullptr) = 0;
  virtual bool loop() = 0;
  virtual DLNAServiceInfo& getService(const char* id) = 0;
  virtual DLNADeviceInfo& getDevice() = 0;
  virtual DLNADeviceInfo& getDevice(int idx) = 0;
  virtual DLNADeviceInfo& getDevice(DLNAServiceInfo& service) = 0;
  virtual DLNADeviceInfo& getDevice(Url location) = 0;
  virtual Vector<DLNADeviceInfo>& getDevices() = 0;
  virtual const char* getUrl(DLNADeviceInfo& device, const char* suffix,
                             char* buffer, int len) = 0;
  virtual bool addDevice(DLNADeviceInfo dev) = 0;
  virtual bool addDevice(Url url) = 0;
  virtual void setActive(bool flag) = 0;
  virtual bool isActive() = 0;
  virtual void setAllowLocalhost(bool flag) = 0;
  virtual ActionReply& getLastReply() = 0;
  virtual SubscriptionMgrControlPoint* getSubscriptionMgr() = 0;
  virtual void setSubscribeNotificationsActive(bool flag) = 0;
  virtual const char* registerString(const char* s) = 0;
};

}  // namespace tiny_dlna
