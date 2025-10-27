// Header-only control point helper for MediaServer devices
#pragma once

#include "dlna/DLNAControlPoint.h"
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
  typedef void (*XMLCallback)(const char* name, const char* test,
                              const char* attributes);
  typedef void (*NotificationCallback)(void* reference, const char* sid,
                                       const char* varName,
                                       const char* newValue);

  /**
   * @brief Construct helper with reference to control point manager
   * @param mgr Reference to DLNAControlPointMgr used to send actions and
   *            manage discovery/subscriptions
   */
  ControlPointMediaServer(DLNAControlPoint& mgr) : mgr(mgr) {}

  /**
   * @brief Begin discovery and processing (forwards to underlying control
   * point)
   * @param http  Http server wrapper used for subscription callbacks
   * @param udp   UDP service used for SSDP discovery
   * @param minWaitMs Minimum time in milliseconds to wait before returning
   * @param maxWaitMs Maximum time in milliseconds to wait for discovery
   * @return true on success, false on error
   */
  bool begin(DLNAHttpRequest& http, IUDPService& udp, uint32_t minWaitMs = 3000,
             uint32_t maxWaitMs = 60000) {
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
   * @brief Select a device by index (0-based) for subsequent actions.
   * By default 0 is used.
   * @param idx Device index
   */
  void setDeviceIndex(int idx) { mgr.setDeviceIndex(idx); }

  /**
   * @brief Subscribe to event notifications for the selected server/device
   * @param timeoutSeconds Subscription timeout in seconds (default: 60)
   * @param cb Optional notification callback; if nullptr the default
   *           `processNotification` will be used
   * @return true on successful SUBSCRIBE, false otherwise
   */
  bool subscribeNotifications(NotificationCallback cb = nullptr,
                              int timeoutSeconds = 3600) {
    DlnaLogger.log(DlnaLogLevel::Debug, "ControlPointMediaServer::subscribeNotifications");
    mgr.setReference(this);
    mgr.setSubscribeInterval(timeoutSeconds);
    // register provided callback or fallback to the default processNotification
    mgr.onNotification(cb != nullptr ? cb : processNotification);
    return mgr.subscribeNotifications();
  }

  /**
   * @brief Restrict this helper to devices of the given device type
   * @param filter Device type string (e.g.
   * "urn:schemas-upnp-org:device:MediaServer:1") Pass nullptr to restore the
   * default filter
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
   * @brief Browse the given object_id and invoke callback for each returned
   * xml element
   * @param startingIndex Starting index for the browse request
   * @param requestedCount Number of items requested
   * @param XMLCallback Callback invoked per MediaItem result (may be nullptr)
   * @param numberReturned Output: number of items returned by server
   * @param totalMatches Output: total matches available on server
   * @param updateID Output: server UpdateID
   * @param browseFlag Optional BrowseFlag (defaults to BrowseDirectChildren)
   * @return true on success, false on error
   * @note The raw DIDL-Lite XML result is available via getLastReply() ->
   * "Result"
   */
  bool browse(int startingIndex, int requestedCount, XMLCallback XMLCallback,
              int& numberReturned, int& totalMatches, int& updateID,
              const char* browseFlag = nullptr) {

    // Register item callback            
    this->mgr.onResultNode(XMLCallback);            
    // Build and post browse action
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return false;
    ActionRequest& act =
        createBrowseAction(svc, browseFlag, startingIndex, requestedCount);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return false;

    // parse numeric result fields
    parseNumericFields(last_reply, numberReturned, totalMatches, updateID);

    return true;
  }

  /**
   * @brief Search the ContentDirectory using SearchCriteria and invoke callback
   * for each returned xml element.
   * @param startingIndex Starting index for the search request
   * @param requestedCount Number of items requested
   * @param XMLCallback Callback invoked per MediaItem result (may be nullptr)
   * @param numberReturned Output: number of items returned by server
   * @param totalMatches Output: total matches available on server
   * @param updateID Output: server UpdateID
   * @param searchCriteria Search criteria string as defined by ContentDirectory
   * @param filter Optional Filter argument (defaults to empty string)
   * @param sortCriteria Optional SortCriteria (defaults to empty string)
   * @return true on success, false on error
   */
  bool search(int startingIndex, int requestedCount, XMLCallback XMLCallback,
              int& numberReturned, int& totalMatches, int& updateID,
              const char* searchCriteria = "", const char* filter = "",
              const char* sortCriteria = "") {

    // Register item callback
    this->mgr.onResultNode(XMLCallback);

    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return false;

    ActionRequest& act =
        createSearchAction(svc, searchCriteria, filter, startingIndex,
                           requestedCount, sortCriteria);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return false;

    parseNumericFields(last_reply, numberReturned, totalMatches, updateID);
    return true;
  }

  /**
   * @brief Get current SystemUpdateID for the ContentDirectory
   * @return numeric SystemUpdateID, or -1 on error
   */
  int getSystemUpdateID() {
    DlnaLogger.log(DlnaLogLevel::Debug, "ControlPointMediaServer::getSystemUpdateID");
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
    DlnaLogger.log(DlnaLogLevel::Debug, "ControlPointMediaServer::getSearchCapabilities");
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
    DlnaLogger.log(DlnaLogLevel::Debug, "ControlPointMediaServer::getSortCapabilities");
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
    DlnaLogger.log(DlnaLogLevel::Debug, "ControlPointMediaServer::getProtocolInfo");
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
  DLNAControlPoint& mgr;
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
    DlnaLogger.log(DlnaLogLevel::Debug, "ControlPointMediaServer::selectService: id='%s'", id);
    // Build list of devices matching the optional filter
    DLNAServiceInfo& s = mgr.getDevice().getService(id);
    if (s) return s;

    // fallback: global search
    return mgr.getService(id);
  }

  /// Find argument value by name in ActionReply
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
  ActionRequest &createBrowseAction(DLNAServiceInfo& svc, const char* browseFlag,
                                   int startingIndex, int requestedCount) {
    static ActionRequest act(svc, "Browse");
    // Use the canonical argument name expected by ContentDirectory: "ObjectID"
    act.addArgument("ObjectID", object_id);
    act.addArgument("BrowseFlag",
                    browseFlag ? browseFlag : "BrowseDirectChildren");
    act.addArgument("Filter", "");
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", startingIndex);
    act.addArgument("StartingIndex", buf);
    snprintf(buf, sizeof(buf), "%d", requestedCount);
    act.addArgument("RequestedCount", buf);
    act.addArgument("SortCriteria", "");
    return act;
  }

  /// Build a Search ActionRequest
  ActionRequest& createSearchAction(DLNAServiceInfo& svc,
                                    const char* searchCriteria,
                                    const char* filter, int startingIndex,
                                    int requestedCount, const char* sortCriteria) {
    static ActionRequest act(svc, "Search");
    act.addArgument("ContainerID", object_id);
    act.addArgument("SearchCriteria", searchCriteria ? searchCriteria : "");
    act.addArgument("Filter", filter ? filter : "");
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", startingIndex);
    act.addArgument("StartingIndex", buf);
    snprintf(buf, sizeof(buf), "%d", requestedCount);
    act.addArgument("RequestedCount", buf);
    act.addArgument("SortCriteria", sortCriteria ? sortCriteria : "");
    return act;
  }

  /// Parse numeric result fields from an ActionReply
  void parseNumericFields(ActionReply& reply, int& numberReturned,
                          int& totalMatches, int& updateID) {
    DlnaLogger.log(DlnaLogLevel::Debug, "ControlPointMediaServer::parseNumericFields");
    const char* nret = findArgument(reply, "NumberReturned");
    const char* tmatch = findArgument(reply, "TotalMatches");
    const char* uid = findArgument(reply, "UpdateID");
    numberReturned = nret ? atoi(nret) : 0;
    totalMatches = tmatch ? atoi(tmatch) : 0;
    updateID = uid ? atoi(uid) : 0;
  }
};

}  // namespace tiny_dlna
