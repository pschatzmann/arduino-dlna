// Header-only control point helper for MediaServer devices
#pragma once

#include "dlna/Action.h"
#include "dlna/DLNAControlPoint.h"

namespace tiny_dlna {

/**
 * @brief Helper class to control/query a MediaServer from a control point
 *
 * Provides convenient wrappers around common ContentDirectory and
 * ConnectionManager actions (Browse, GetSystemUpdateID, GetSearchCapabilities,
 * GetProtocolInfo). Methods are synchronous and return boolean success or
 * populate output parameters.
 */
class DLNAControlPointMediaServer {
 public:
  typedef void (*XMLCallback)(const char* name, const char* test,
                              const char* attributes);
  typedef void (*NotificationCallback)(void* reference, const char* sid,
                                       const char* varName,
                                       const char* newValue);

  /// Default constructor
  DLNAControlPointMediaServer() = default;
  /**
   * @brief Construct helper with references to control point manager and
   *        required transport instances (HTTP for subscribe callbacks and
   *        UDP for SSDP discovery)
   * @param mgr Reference to DLNAControlPointMgr used to send actions and
   *            manage discovery/subscriptions
   * @param http  Http server wrapper used for subscription callbacks
   * @param udp   UDP service used for SSDP discovery
   */
  DLNAControlPointMediaServer(DLNAControlPoint& mgr, DLNAHttpRequest& http,
                              IUDPService& udp)
      : p_mgr(&mgr), p_http(&http), p_udp(&udp) {}

  /// Set the control point manager instance (required before using helper)
  void setDLNAControlPoint(DLNAControlPoint& m) { p_mgr = &m; }

  /**
   * @brief Begin discovery and processing (forwards to underlying control
   * point)
   * @param minWaitMs Minimum time in milliseconds to wait before returning
   * @param maxWaitMs Maximum time in milliseconds to wait for discovery
   * @return true on success, false on error
   */
  bool begin(uint32_t minWaitMs = 3000, uint32_t maxWaitMs = 60000) {
    DlnaLogger.log(
        DlnaLogLevel::Info,
        "DLNAControlPointMediaServer::begin: device_type_filter='%s'",
        device_type_filter ? device_type_filter : "(null)");
    if (!p_mgr) {
      DlnaLogger.log(DlnaLogLevel::Error, "mgr instance not set");
      return false;
    }
    if (!p_http) {
      DlnaLogger.log(DlnaLogLevel::Error, "http instance not set");
      return false;
    }
    if (!p_udp) {
      DlnaLogger.log(DlnaLogLevel::Error, "udp instance not set");
      return false;
    }
    return p_mgr->begin(*p_http, *p_udp, device_type_filter, minWaitMs,
                        maxWaitMs);
  }

  /// Setter for the HTTP wrapper used for subscriptions and callbacks
  void setHttp(DLNAHttpRequest& http) { p_http = &http; }

  /// Setter for UDP service used for discovery (SSDP)
  void setUdp(IUDPService& udp) { p_udp = &udp; }

  /**
   * @brief Return number of discovered devices known to the control point
   * @return number of devices matching the optional device type filter
   */
  int getDeviceCount() {
    // count only devices matching the device_type_filter
    int count = 0;
    for (auto& d : p_mgr->getDevices()) {
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
  void setDeviceIndex(int idx) { p_mgr->setDeviceIndex(idx); }

  /**
   * @brief Subscribe to event notifications for the selected server/device
   * @param timeoutSeconds Subscription timeout in seconds (default: 60)
   * @param cb Optional notification callback; if nullptr the default
   *           `processNotification` will be used
   * @return true on successful SUBSCRIBE, false otherwise
   */
  bool subscribeNotifications(NotificationCallback cb = nullptr,
                              int timeoutSeconds = 3600) {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "ControlPointMediaServer::subscribeNotifications");
    p_mgr->setReference(this);
    p_mgr->setSubscribeInterval(timeoutSeconds);
    // register provided callback or fallback to the default processNotification
    p_mgr->onNotification(cb != nullptr ? cb : processNotification);
    return p_mgr->subscribeNotifications();
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
  bool browse(int startingIndex, int requestedCount, ContentQueryType queryType,
              XMLCallback XMLCallback, int& numberReturned, int& totalMatches,
              int& updateID) {
    // Register item callback
    this->p_mgr->onResultNode(XMLCallback);
    // Build and post browse action
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return false;
    ActionRequest& act =
        createBrowseAction(svc, queryType, startingIndex, requestedCount);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
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
    this->p_mgr->onResultNode(XMLCallback);

    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return false;

    ActionRequest& act =
        createSearchAction(svc, searchCriteria, filter, startingIndex,
                           requestedCount, sortCriteria);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return false;

    parseNumericFields(last_reply, numberReturned, totalMatches, updateID);
    return true;
  }

  /**
   * @brief Get current SystemUpdateID for the ContentDirectory
   * @return numeric SystemUpdateID, or -1 on error
   */
  int getSystemUpdateID() {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "ControlPointMediaServer::getSystemUpdateID");
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return -1;
    ActionRequest act(svc, "GetSystemUpdateID");
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return -1;
    const char* v = last_reply.findArgument("Id");
    if (!v) v = last_reply.findArgument("Id");
    return v ? atoi(v) : -1;
  }

  /**
   * @brief Get the ContentDirectory SearchCapabilities string
   * @return pointer to capability string owned by reply, or nullptr on error
   */
  const char* getSearchCapabilities() {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "ControlPointMediaServer::getSearchCapabilities");
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetSearchCapabilities");
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return nullptr;
    return last_reply.findArgument("SearchCaps");
  }

  /**
   * @brief Get the ContentDirectory SortCapabilities string
   * @return pointer to sort capabilities, or nullptr on error
   */
  const char* getSortCapabilities() {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "ControlPointMediaServer::getSortCapabilities");
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetSortCapabilities");
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return nullptr;
    return last_reply.findArgument("SortCaps");
  }

  /**
   * @brief Query ConnectionManager:GetProtocolInfo
   * @return pointer to protocol info string (Source), or nullptr on error
   */
  const char* getProtocolInfo() {
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "ControlPointMediaServer::getProtocolInfo");
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:ConnectionManager");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetProtocolInfo");
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return nullptr;
    return last_reply.findArgument("Source");
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
  DLNAControlPoint* p_mgr = nullptr;
  const char* device_type_filter_default =
      "urn:schemas-upnp-org:device:MediaServer:1";
  const char* device_type_filter = device_type_filter_default;
  ActionReply last_reply;
  StringRegistry strings;
  void* reference = nullptr;
  const char* object_id = "0";
  // transport references (optional) - must be set before calling begin()
  DLNAHttpRequest* p_http = nullptr;
  IUDPService* p_udp = nullptr;

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
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "ControlPointMediaServer::selectService: id='%s'", id);
    // Build list of devices matching the optional filter
    DLNAServiceInfo& s = p_mgr->getDevice().getService(id);
    if (s) return s;

    // fallback: global search
    return p_mgr->getService(id);
  }

  /// Build a Browse ActionRequest
  ActionRequest& createBrowseAction(DLNAServiceInfo& svc,
                                    ContentQueryType queryType,
                                    int startingIndex, int requestedCount) {
    const char* browseFlag = (queryType == ContentQueryType::BrowseMetadata)
                                 ? "BrowseMetadata"
                                 : "BrowseDirectChildren";
    static ActionRequest act(svc, "Browse");
    // Use the canonical argument name expected by ContentDirectory: "ObjectID"
    act.addArgument("ObjectID", object_id);
    act.addArgument("BrowseFlag", browseFlag);
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
                                    int requestedCount,
                                    const char* sortCriteria) {
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
    DlnaLogger.log(DlnaLogLevel::Debug,
                   "ControlPointMediaServer::parseNumericFields");
    const char* nret = reply.findArgument("NumberReturned");
    const char* tmatch = reply.findArgument("TotalMatches");
    const char* uid = reply.findArgument("UpdateID");
    numberReturned = nret ? atoi(nret) : 0;
    totalMatches = tmatch ? atoi(tmatch) : 0;
    updateID = uid ? atoi(uid) : 0;
  }
};

}  // namespace tiny_dlna
