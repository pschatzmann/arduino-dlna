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

  ControlPointMediaServer(DLNAControlPointMgr& mgr) : mgr(mgr) {}

  /// Return number of discovered devices known to the control point
  int getDeviceCount() {
    // count only devices matching the device_type_filter
    int count = 0;
    for (auto &d : mgr.getDevices()) {
      const char* dt = d.getDeviceType();
      if (!device_type_filter || StrView(dt).contains(device_type_filter)) count++;
    }
    return count;
  }

  /// Returns the reply from the last request
  const ActionReply& getLastReply() const { return last_reply; }

  /// Selects a device by index
  void setDeviceIndex(int idx) { device_index = idx; }

  /// Restrict this control helper to devices of the given device type
  /// e.g. "urn:schemas-upnp-org:device:MediaServer:1". Pass nullptr to
  /// disable filtering.
  void setDeviceTypeFilter(const char* filter) { device_type_filter = filter; }

  /// Attach an opaque reference pointer (optional, for caller context)
  void setReference(void* ref) { reference = ref; }

  /**
   * Browse the given objectID and fill the provided results vector with
   * MediaServer::MediaItem entries. Returns true on success.
   * NOTE: This implementation delegates actual parsing of the DIDL result to
   * the caller. Here we return the raw Result XML string in last_reply so the
   * caller can parse it, or a helper parser can be added later.
   */
  // itemCallback will be called for each parsed MediaItem. The opaque
  // reference (set via setReference) is passed as second parameter.
  bool browse(const char* objectID, const char* browseFlag, int startingIndex,
              int requestedCount, ItemCallback itemCallback,
              int& numberReturned, int& totalMatches, int& updateID) {
    // Build and post browse action
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:ContentDirectory");
    if (!svc) return false;
    ActionRequest act = createBrowseAction(svc, objectID, browseFlag, startingIndex, requestedCount);
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

  // Get the current system update ID for the ContentDirectory
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

  // Get the search capabilities string
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

  // Get the sort capabilities string
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

  // GetProtocolInfo from ConnectionManager
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

 protected:
  // Select the service from the device at device_index when possible. If the
  // index is out of range or the device does not expose the requested
  // service, fall back to a global search across all discovered devices.
  DLNAServiceInfo& selectService(const char* id) {
    // Build list of devices matching the optional filter
    Vector<DLNADevice>& all = mgr.getDevices();
    int idx = 0;
    for (auto &d : all) {
      const char* dt = d.getDeviceType();
      if (device_type_filter && !StrView(dt).contains(device_type_filter)) continue;
      if (idx == device_index) {
        DLNAServiceInfo& s = d.getService(id);
        if (s) return s;
      }
      idx++;
    }

    // fallback: global search
    return mgr.getService(id);
  }
  DLNAControlPointMgr& mgr;
  int device_index = 0;
  const char* device_type_filter = "urn:schemas-upnp-org:device:MediaServer:1";
  ActionReply last_reply;
  StringRegistry strings;
  void* reference = nullptr;

  const char* findArgument(ActionReply& r, const char* name) {
    for (auto& a : r.arguments) {
      if (a.name != nullptr) {
        StrView nm(a.name);
        if (nm == name) return a.value.c_str();
      }
    }
    return nullptr;
  }

  // Build a Browse ActionRequest
  ActionRequest createBrowseAction(DLNAServiceInfo& svc, const char* objectID,
                                   const char* browseFlag, int startingIndex,
                                   int requestedCount) {
    ActionRequest act(svc, "Browse");
    act.addArgument("ObjectID", objectID ? objectID : "0");
    act.addArgument("BrowseFlag", browseFlag ? browseFlag : "BrowseDirectChildren");
    act.addArgument("Filter", "*");
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", startingIndex);
    act.addArgument("StartingIndex", buf);
    snprintf(buf, sizeof(buf), "%d", requestedCount);
    act.addArgument("RequestedCount", buf);
    act.addArgument("SortCriteria", "");
    return act;
  }

  // Parse numeric result fields from an ActionReply
  void parseNumericFields(ActionReply& reply, int& numberReturned, int& totalMatches, int& updateID) {
    const char* nret = findArgument(reply, "NumberReturned");
    const char* tmatch = findArgument(reply, "TotalMatches");
    const char* uid = findArgument(reply, "UpdateID");
    numberReturned = nret ? atoi(nret) : 0;
    totalMatches = tmatch ? atoi(tmatch) : 0;
    updateID = uid ? atoi(uid) : 0;
  }

  // Parse DIDL-Lite Result and invoke callback for each item
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
