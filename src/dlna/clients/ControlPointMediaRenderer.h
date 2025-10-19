// Header-only control point helper for MediaRenderer devices
#pragma once

#include "dlna/DLNAControlPointMgr.h"
#include "dlna/service/Action.h"
#include <functional>

namespace tiny_dlna {

/**
 * @brief Helper class to control a MediaRenderer device from a control point
 *
 * This lightweight helper uses the DLNAControlPointMgr action API to send
 * common AVTransport and RenderingControl actions to the first MediaRenderer
 * service discovered. It is intentionally minimal and returns boolean
 * success/failure for each operation.
 */
class ControlPointMediaRenderer {
 public:
  ControlPointMediaRenderer(DLNAControlPointMgr& mgr) : mgr(mgr) {}
  /// Restrict this helper to devices of the given device type (default:
  /// MediaRenderer)
  void setDeviceTypeFilter(const char* filter) {
    device_type_filter = filter ? filter : device_type_filter_default;
  }

  /// Begin discovery / processing — forwards to the underlying control point
  bool begin(DLNAHttpRequest& http, IUDPService& udp,
             uint32_t processingTime = 0, bool stopWhenFound = true) {
    DlnaLogger.log(DlnaLogLevel::Info,
                   "ControlPointMediaServer::begin device_type_filter='%s'",
                   device_type_filter ? device_type_filter : "(null)");
    return mgr.begin(http, udp, device_type_filter, processingTime,
                     stopWhenFound);
  }

  /// Return number of discovered devices matching the renderer filter
  int getDeviceCount() {
    int count = 0;
    for (auto& d : mgr.getDevices()) {
      const char* dt = d.getDeviceType();
      if (!device_type_filter || StrView(dt).contains(device_type_filter))
        count++;
    }
    return count;
  }

  /// Set the active renderer by index (optional). Default uses first matching
  /// service.
  void setDeviceIndex(int idx) { device_index = idx; }

  /// Subscribe to event notifications for the selected renderer
  bool subscribeNotifications(int duration = 60) {
    mgr.setReference(this);
    mgr.onNotification(processNotification);
    auto& device = mgr.getDevice(device_index);
    return mgr.subscribeNotifications(device, duration);
  }

  /// Set the InstanceID used for action requests (default: "0")
  void setInstanceID(const char* id) { instance_id = id; }

  /// Set media URI on the renderer (synchronous)
  bool setMediaURI(const char* uri) {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) {
      DlnaLogger.log(DlnaLogLevel::Error, "No AVTransport service found");
      return false;
    }
    ActionRequest act(svc, "SetAVTransportURI");
    act.addArgument("InstanceID", instance_id);
    act.addArgument("CurrentURI", uri);
    act.addArgument("CurrentURIMetaData", "");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    return (bool)last_reply;
  }

  /// Play (speed defaults to "1")
  bool play() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) {
      DlnaLogger.log(DlnaLogLevel::Error, "No AVTransport service found");
      return false;
    }
    ActionRequest act(svc, "Play");
    act.addArgument("InstanceID", instance_id);
    act.addArgument("Speed", "1");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (last_reply) setActiveState(true);
    return (bool)last_reply;
  }

  /// Play a given URI
  bool play(const char* uri) {
    if (!setMediaURI(uri)) return false;
    return play();
  }

  /// Pause
  bool pause() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return false;
    ActionRequest act(svc, "Pause");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (last_reply) setActiveState(false);
    return (bool)last_reply;
  }

  /// Stop
  bool stop() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return false;
    ActionRequest act(svc, "Stop");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (last_reply) setActiveState(false);
    return (bool)last_reply;
  }

  /// Set volume (0..100)
  bool setVolume(int volumePercent) {
    if (volumePercent < 0) volumePercent = 0;
    if (volumePercent > 100) volumePercent = 100;
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:RenderingControl");
    if (!svc) {
      DlnaLogger.log(DlnaLogLevel::Error, "No RenderingControl service found");
      return false;
    }
    char volBuf[8];
    snprintf(volBuf, sizeof(volBuf), "%d", volumePercent);
    ActionRequest act(svc, "SetVolume");
    act.addArgument("InstanceID", "0");
    act.addArgument("Channel", "Master");
    act.addArgument("DesiredVolume", volBuf);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    return (bool)last_reply;
  }

  /// Set mute state (true = muted)
  bool setMute(bool mute) {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:RenderingControl");
    if (!svc) return false;
    ActionRequest act(svc, "SetMute");
    act.addArgument("InstanceID", "0");
    act.addArgument("Channel", "Master");
    act.addArgument("DesiredMute", mute ? "1" : "0");
    mgr.addAction(act);
    ActionReply r = mgr.executeActions();
    return (bool)r;
  }

  /**
   * @brief Query the current volume from the renderer
   * @return volume 0..100, or -1 on error
   */
  int getVolume() {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:RenderingControl");
    if (!svc) return -1;
    ActionRequest act(svc, "GetVolume");
    act.addArgument("InstanceID", instance_id);
    act.addArgument("Channel", "Master");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return -1;
    const char* v = findArgument(last_reply, "CurrentVolume");
    if (!v) return -1;
    return atoi(v);
  }

  /**
   * @brief Query mute state
   * @return 0 = unmuted, 1 = muted, -1 = error/unknown
   */
  int getMute() {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:RenderingControl");
    if (!svc) return -1;
    ActionRequest act(svc, "GetMute");
    act.addArgument("InstanceID", instance_id);
    act.addArgument("Channel", "Master");
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return -1;
    const char* m = findArgument(last_reply, "CurrentMute");
    if (!m) return -1;
    return atoi(m);
  }

  /**
   * @brief Query current playback position (RelTime) from AVTransport
   * @return position in milliseconds, or 0 if unknown/error
   */
  unsigned long getPositionMs() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return 0;
    ActionRequest act(svc, "GetPositionInfo");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return 0;
    const char* rel = findArgument(last_reply, "RelTime");
    if (!rel) return 0;
    return parseTimeToMs(rel);
  }

  /**
   * @brief Query transport state (e.g. STOPPED, PLAYING, PAUSED_PLAYBACK)
   * @return pointer to string (owned by StringRegistry) or nullptr on error
   */
  const char* getTransportState() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetTransportInfo");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return nullptr;
    return findArgument(last_reply, "CurrentTransportState");
  }

  /**
   * @brief Get the current media URI
   * @return pointer to URI string or nullptr
   */
  const char* getCurrentURI() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetMediaInfo");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return nullptr;
    return findArgument(last_reply, "CurrentURI");
  }

  /**
   * @brief Get number of tracks in the current media
   * @return number of tracks or -1 on error
   */
  int getNrTracks() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return -1;
    ActionRequest act(svc, "GetMediaInfo");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return -1;
    const char* n = findArgument(last_reply, "NrTracks");
    if (!n) return -1;
    return atoi(n);
  }

  /**
   * @brief Get total media duration as milliseconds (from MediaDuration)
   * @return milliseconds or 0 on error/unknown
   */
  unsigned long getMediaDurationMs() {
    DLNAServiceInfo& svc = mgr.getService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return 0;
    ActionRequest act(svc, "GetMediaInfo");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return 0;
    const char* d = findArgument(last_reply, "MediaDuration");
    if (!d) return 0;
    return parseTimeToMs(d);
  }

  /**
   * @brief Get current track index (from GetPositionInfo)
   * @return track index (numeric) or -1 on error
   */
  int getTrackIndex() {
    DLNAServiceInfo& svc = mgr.getService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return -1;
    ActionRequest act(svc, "GetPositionInfo");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return -1;
    const char* tr = findArgument(last_reply, "Track");
    if (!tr) return -1;
    return atoi(tr);
  }

  /**
   * @brief Get current track duration in milliseconds (from TrackDuration)
   * @return milliseconds or 0 on error
   */
  unsigned long getTrackDurationMs() {
    DLNAServiceInfo& svc = mgr.getService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return 0;
    ActionRequest act(svc, "GetPositionInfo");
    act.addArgument("InstanceID", instance_id);
    mgr.addAction(act);
    last_reply = mgr.executeActions();
    if (!last_reply) return 0;
    const char* td = findArgument(last_reply, "TrackDuration");
    if (!td) return 0;
    return parseTimeToMs(td);
  }

  /// Return last ActionReply (from the most recent call)
  const ActionReply& getLastReply() const { return last_reply; }

  /// Returns true if we are currently playing
  bool isActive() { return is_active; }

  /// Defines a reference object that will be provided by the callbacks
  void setReference(void* ref) { reference = ref; }

  /// Register a callback invoked when the active (playing) state changes
  /// The callback receives the new active state as first parameter and
  /// the opaque reference (set via setReference) as second parameter.
  void onActiveChanged(std::function<void(bool, void*)> cb) { activeChangedCallback = cb; }

 protected:
  DLNAControlPointMgr& mgr;
  bool is_active = false;
  std::function<void(bool, void*)> activeChangedCallback = nullptr;
  int device_index = 0;
  const char* instance_id = "0";
  const char* device_type_filter_default =
      "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* device_type_filter = device_type_filter_default;
  ActionReply last_reply;
  void *reference = nullptr;

  // Helper to update is_active and notify callback only on change
  void setActiveState(bool s) {
    if (is_active == s) return;
    is_active = s;
    DlnaLogger.log(DlnaLogLevel::Info, "ControlPointMediaRenderer active=%s", is_active ? "true" : "false");
    if (activeChangedCallback) activeChangedCallback(is_active, reference);
  }

  /// Notification callback
  static void processNotification(void* reference, const char* sid,
                                  const char* varName, const char* newValue) {
    // Log every notification invocation for debugging/visibility
    DlnaLogger.log(DlnaLogLevel::Info,
                   "processNotification sid='%s' var='%s' value='%s'",
                   sid ? sid : "(null)", varName ? varName : "(null)",
                   newValue ? newValue : "(null)");

    ControlPointMediaRenderer* renderer =
        static_cast<ControlPointMediaRenderer*>(reference);
    if (renderer) {
      // Handle the notification (e.g., update internal state, call callbacks)
      if (StrView(varName) == "TransportState") {
        if (StrView(newValue) == "PLAYING") {
          renderer->setActiveState(true);
        } else {
          renderer->setActiveState(false);
        }
      }
    }
  }

  // Helper: find argument by name in ActionReply (returns nullptr if not found)
  const char* findArgument(ActionReply& r, const char* name) {
    for (auto& a : r.arguments) {
      if (a.name != nullptr) {
        StrView nm(a.name);
        if (nm == name) return a.value.c_str();
      }
    }
    return nullptr;
  }

  // Helper: parse DLNA time string (HH:MM:SS, MM:SS) to milliseconds
  static unsigned long parseTimeToMs(const char* t) {
    if (t == nullptr) return 0;
    int h = 0, m = 0, s = 0;
    int parts = 0;
    // Count colons
    const char* p = t;
    int colonCount = 0;
    while (*p) {
      if (*p == ':') colonCount++;
      p++;
    }
    if (colonCount == 2) {
      parts = sscanf(t, "%d:%d:%d", &h, &m, &s);
    } else if (colonCount == 1) {
      parts = sscanf(t, "%d:%d", &m, &s);
      h = 0;
    } else {
      // maybe it's seconds only
      parts = sscanf(t, "%d", &s);
    }
    if (parts <= 0) return 0;
    unsigned long total =
        (unsigned long)h * 3600UL + (unsigned long)m * 60UL + (unsigned long)s;
    return total * 1000UL;
  }

  // Select service on a device filtered by device_type_filter
  DLNAServiceInfo& selectService(const char* id) {
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
    return mgr.getService(id);
  }
};

}  // namespace tiny_dlna
