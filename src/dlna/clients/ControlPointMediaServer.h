// Header-only control point helper for MediaServer devices
#pragma once

#include "dlna/DLNAControlPointMgr.h"
#include "dlna/devices/MediaServer/MediaServer.h"
#include "dlna/service/Action.h"

namespace tiny_dlna {

/**
 * @brief Helper class to control/query a MediaServer from a control point
 *
 * Provides convenient wrappers around common ContentDirectory and
 * ConnectionManager actions (Browse, GetSystemUpdateID, GetSearchCapabilities,
 * GetProtocolInfo). Methods are synchronous and return boolean success or
 * populate output parameters.
 */
class ControlPointMediaServer {
 public:
  typedef void (*ItemCallback)(const MediaItem& item, void* ref);
  typedef void (*NotificationCallback)(void* reference, const char* sid,
                                       const char* varName,
                                       const char* newValue);

  /**
   * @brief Construct helper with reference to control point manager
   * @param mgr Reference to DLNAControlPointMgr used to send actions and
   *            manage discovery/subscriptions
   */
  ControlPointMediaServer(DLNAControlPointMgr& mgr) : mgr(mgr) {}

  /**
   * @brief Begin discovery and processing (forwards to underlying control point)
   * @param http  Http server wrapper used for subscription callbacks
   * @param udp   UDP service used for SSDP discovery
   * @param minWaitMs Minimum time in milliseconds to wait before returning
   * @param maxWaitMs Maximum time in milliseconds to wait for discovery
   * @return true on success, false on error
   */
  bool begin(DLNAHttpRequest& http, IUDPService& udp,
             uint32_t minWaitMs = 3000, uint32_t maxWaitMs = 60000) {
    DlnaLogger.log(DlnaLogLevel::Info,
                   "ControlPointMediaServer::begin device_type_filter='%s'",
                   device_type_filter ? device_type_filter : "(null)");
    return mgr.begin(http, udp, device_type_filter, minWaitMs, maxWaitMs);
  }

  /**
   * @brief Return number of discovered devices known to the control point
   * @return number of devices matching the optional device type filter
   */
  int getDeviceCount() {
    // count only devices matching the device_type_filter
    int count = 0;
    for (auto& d : mgr.getDevices()) {
      const char* dt = d.getDeviceType();
      if (!device_type_filter || StrView(dt).contains(device_type_filter))
        count++;
    }
    return count;
  }

  /**
   * @brief Return the ActionReply from the last synchronous request
   * @return reference to the last ActionReply
   */
  const ActionReply& getLastReply() const { return last_reply; }

  /**
   * @brief Select a device by index (0-based) for subsequent actions
   * @param idx Device index
   */
  void setDeviceIndex(int idx) { device_index = idx; }

  /**
   * @brief Subscribe to event notifications for the selected server/device
   * @param timeoutSeconds Subscription timeout in seconds (default: 60)
   * @param cb Optional notification callback; if nullptr the default
   *           `processNotification` will be used
   * @return true on successful SUBSCRIBE, false otherwise
   */
  bool subscribeNotifications(int timeoutSeconds = 60,
                              NotificationCallback cb = nullptr) {
    mgr.setReference(this);
    // register provided callback or fallback to the default processNotification
    if (cb)
      mgr.onNotification(cb);
    else
      mgr.onNotification(processNotification);
    auto& device = mgr.getDevice(device_index);
    return mgr.subscribeNotifications(device, timeoutSeconds);
  }

  /**
   * @brief Restrict this helper to devices of the given device type
   * @param filter Device type string (e.g. "urn:schemas-upnp-org:device:MediaServer:1")
   *               Pass nullptr to restore the default filter
   */
  void setDeviceTypeFilter(const char* filter) {
    device_type_filter = filter ? filter : device_type_filter_default;
  }

  /**
   * @brief Attach an opaque reference pointer passed to callbacks
   * @param ref Opaque user pointer
   */
  void setReference(void* ref) { reference = ref; }

  /**
   * @brief Browse the given object_id and invoke callback for each returned item
   * @param startingIndex Starting index for the browse request
   * @param requestedCount Number of items requested
   * @param itemCallback Callback invoked per MediaItem result (may be nullptr)
   * @param numberReturned Output: number of items returned by server
   * @param totalMatches Output: total matches available on server
   * @param updateID Output: server UpdateID
   * @param browseFlag Optional BrowseFlag (defaults to BrowseDirectChildren)
   * @return true on success, false on error
   * @note The raw DIDL-Lite XML result is available via getLastReply() -> "Result"
   */
  bool browse(int startingIndex, int requestedCount, ItemCallback itemCallback,
              int& numberReturned, int& totalMatches, int& updateID,
              const char* browseFlag = nullptr) {
    // Build and post browse action
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return false;
    ActionRequest act =
        createBrowseAction(svc, browseFlag, startingIndex, requestedCount);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return false;

    // parse numeric result fields
    parseNumericFields(last_reply, numberReturned, totalMatches, updateID);

    // process DIDL Result and call callback per item
    const char* resultXml = findArgument(last_reply, "Result");
    if (resultXml) processResult(resultXml, itemCallback);
    return true;
  }

  /**
   * @brief Get current SystemUpdateID for the ContentDirectory
   * @return numeric SystemUpdateID, or -1 on error
   */
  int getSystemUpdateID() {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return -1;
    ActionRequest act(svc, "GetSystemUpdateID");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return -1;
    const char* v = findArgument(last_reply, "Id");
    if (!v) v = findArgument(last_reply, "Id");
    return v ? atoi(v) : -1;
  }

  /**
   * @brief Get the ContentDirectory SearchCapabilities string
   * @return pointer to capability string owned by reply, or nullptr on error
   */
  const char* getSearchCapabilities() {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetSearchCapabilities");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return nullptr;
    return findArgument(last_reply, "SearchCaps");
  }

  /**
   * @brief Get the ContentDirectory SortCapabilities string
   * @return pointer to sort capabilities, or nullptr on error
   */
  const char* getSortCapabilities() {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetSortCapabilities");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return nullptr;
    return findArgument(last_reply, "SortCaps");
  }

  /**
   * @brief Query ConnectionManager:GetProtocolInfo
   * @return pointer to protocol info string (Source), or nullptr on error
   */
  const char* getProtocolInfo() {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ConnectionManager");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetProtocolInfo");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return nullptr;
    return findArgument(last_reply, "Source");
  }

  /**
   * @brief Set the object id used for browse operations (default: "0")
   * @param id C-string object id
   */
  void setObjectID(const char* id) { object_id = id; }

  /**
   * @brief Return the current object id used for browse
   * @return C-string object id
   */
  const char* getObjectID() const { return object_id; }

 protected:
  DLNAControlPointMgr& mgr;
  int device_index = 0;
  const char* device_type_filter_default =
      "urn:schemas-upnp-org:device:MediaServer:1";
  const char* device_type_filter = device_type_filter_default;
  ActionReply last_reply;
  StringRegistry strings;
  void* reference = nullptr;
  const char* object_id = "0";

  /// Notification callback: just log for now
  static void processNotification(void* reference, const char* sid,
                                  const char* varName, const char* newValue) {
    // Log every notification invocation for debugging/visibility
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processNotification sid='%s' var='%s' value='%s'",
                   sid ? sid : "(null)", varName ? varName : "(null)",
                   newValue ? newValue : "(null)");
  }

  /// Select service by id
  DLNAServiceInfo& selectService(const char* id) {
    // Build list of devices matching the optional filter
    Vector<DLNADevice>& all = mgr.getDevices();
    int idx = 0;
    for (auto& d : all) {
      const char* dt = d.getDeviceType();
      if (device_type_filter && !StrView(dt).contains(device_type_filter))
        continue;
      if (idx == device_index) {
        DLNAServiceInfo& s = d.getService(id);
        if (s) return s;
      }
      idx++;
    }

    // fallback: global search
    return mgr.getService(id);
  }

  const char* findArgument(ActionReply& r, const char* name) {
    for (auto& a : r.arguments) {
      if (a.name != nullptr) {
        StrView nm(a.name);
        if (nm == name) return a.value.c_str();
      }
    }
    return nullptr;
  }

  /// Build a Browse ActionRequest
  ActionRequest createBrowseAction(DLNAServiceInfo& svc, const char* browseFlag,
                                   int startingIndex, int requestedCount) {
    ActionRequest act(svc, "Browse");
    act.addArgument("object_id", object_id);
    act.addArgument("BrowseFlag",
                    browseFlag ? browseFlag : "BrowseDirectChildren");
    act.addArgument("Filter", "*");
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", startingIndex);
    act.addArgument("StartingIndex", buf);
    snprintf(buf, sizeof(buf), "%d", requestedCount);
    act.addArgument("RequestedCount", buf);
    act.addArgument("SortCriteria", "");
    return act;
  }

  /// Parse numeric result fields from an ActionReply
  void parseNumericFields(ActionReply& reply, int& numberReturned,
                          int& totalMatches, int& updateID) {
    const char* nret = findArgument(reply, "NumberReturned");
    const char* tmatch = findArgument(reply, "TotalMatches");
    const char* uid = findArgument(reply, "UpdateID");
    numberReturned = nret ? atoi(nret) : 0;
    totalMatches = tmatch ? atoi(tmatch) : 0;
    updateID = uid ? atoi(uid) : 0;
  }

  /// Parse DIDL-Lite Result and invoke callback for each item
  void processResult(const char* resultXml, ItemCallback itemCallback) {
    if (!resultXml) return;
    StrView res(resultXml);
    int pos = 0;
    while (true) {
      int itPos = res.indexOf("<item", pos);
      if (itPos < 0) break;
      int itEnd = res.indexOf("</item>", itPos);
      if (itEnd < 0) break;
      int headerEnd = res.indexOf('>', itPos);
      if (headerEnd < 0 || headerEnd >= itEnd) break;

      tiny_dlna::MediaItem item;
      // extract id attribute
      int idPos = res.indexOf("id=\"", itPos);
      if (idPos >= 0 && idPos < headerEnd) {
        int valStart = idPos + 4;
        int valEnd = res.indexOf('"', valStart);
        if (valEnd > valStart) {
          Str tmp;
          tmp.copyFrom(res.c_str() + valStart, valEnd - valStart);
          item.id = strings.add((char*)tmp.c_str());
        }
      }

      // title
      int t1 = res.indexOf("<dc:title>", headerEnd);
      if (t1 >= 0 && t1 < itEnd) {
        int t1s = t1 + strlen("<dc:title>");
        int t1e = res.indexOf("</dc:title>", t1s);
        if (t1e > t1s) {
          Str tmp;
          tmp.copyFrom(res.c_str() + t1s, t1e - t1s);
          item.title = strings.add((char*)tmp.c_str());
        }
      }

      // res
      int r1 = res.indexOf("<res", headerEnd);
      if (r1 >= 0 && r1 < itEnd) {
        int r1s = res.indexOf('>', r1);
        if (r1s >= 0 && r1s < itEnd) {
          r1s += 1;
          int r1e = res.indexOf("</res>", r1s);
          if (r1e > r1s) {
            Str tmp;
            tmp.copyFrom(res.c_str() + r1s, r1e - r1s);
            item.res = strings.add((char*)tmp.c_str());
          }
        }
      }

      if (itemCallback) itemCallback(item, reference);
      pos = itEnd + 7;
    }
  }
};

}  // namespace tiny_dlna
