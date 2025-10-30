// Header-only control point helper for MediaRenderer devices
#pragma once

#include <functional>

#include "dlna/Action.h"
#include "dlna/DLNAControlPoint.h"

namespace tiny_dlna {

/**
 * @brief Helper class to control a MediaRenderer device from a control point
 *
 * This lightweight helper uses the DLNAControlPointMgr action API to send
 * common AVTransport and RenderingControl actions to the first MediaRenderer
 * service discovered. It is intentionally minimal and returns boolean
 * success/failure for each operation.
 */
class DLNAControlPointMediaRenderer {
 public:
  /// Default constructor
  DLNAControlPointMediaRenderer() = default;

  /**
   * @brief Construct helper with references to control point manager and
   *        transport instances.
   *
   * This convenience constructor binds the underlying `DLNAControlPoint`
   * manager as well as the HTTP wrapper (used for subscription callbacks)
   * and the UDP service (used for SSDP discovery). It is equivalent to
   * default-constructing the helper and then calling `setDLNAControlPoint()`,
   * `setHttp()` and `setUdp()`, but more concise for callers that already
   * have the instances available.
   *
   * @param m Reference to the `DLNAControlPoint` manager
   * @param http Reference to an HTTP request wrapper used for subscriptions
   * @param udp Reference to a UDP service used for SSDP discovery
   */
  DLNAControlPointMediaRenderer(DLNAControlPoint& m, DLNAHttpRequest& http,
                                IUDPService& udp)
      : p_mgr(&m), p_http(&http), p_udp(&udp) {}

  /// Set the control point manager instance (required before using helper)
  void setDLNAControlPoint(DLNAControlPoint& m) { p_mgr = &m; }

  /**
   * @brief Restrict this helper to devices of the given device type
   * @param filter Device type filter, e.g.
   * "urn:schemas-upnp-org:device:MediaRenderer:1". Pass nullptr to use the
   * default MediaRenderer filter.
   */
  void setDeviceTypeFilter(const char* filter) {
    device_type_filter = filter ? filter : device_type_filter_default;
  }

  /**
   * @brief Begin discovery and processing
   * @param http  Http server wrapper used for subscription callbacks
   * @param udp   UDP service used for SSDP discovery
   * @param minWaitMs Minimum time in milliseconds to wait before returning
   * @param maxWaitMs Maximum time in milliseconds to wait for discovery
   * @return true on success, false on error
   */
  bool begin(uint32_t minWaitMs = 3000, uint32_t maxWaitMs = 60000) {
    DlnaLogger.log(DlnaLogLevel::Info,
                   "ControlPointMediaRenderer::begin - device_type_filter='%s'",
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

  /**
   * @brief Return the number of discovered devices matching the renderer filter
   * @return number of matching devices
   */
  int getDeviceCount() {
    int count = 0;
    for (auto& d : p_mgr->getDevices()) {
      const char* dt = d.getDeviceType();
      if (!device_type_filter || StrView(dt).contains(device_type_filter))
        count++;
    }
    return count;
  }

  /**
   * @brief Select the active renderer by index (0-based)
   * @param idx Index of the device to select; defaults to 0
   */
  void setDeviceIndex(int idx) { device_index = idx; }

  /// Setter for the HTTP wrapper used for subscriptions and callbacks
  void setHttp(DLNAHttpRequest& http) { p_http = &http; }

  /// Setter for UDP service used for discovery (SSDP)
  void setUdp(IUDPService& udp) { p_udp = &udp; }

  typedef void (*NotificationCallback)(void* reference, const char* sid,
                                       const char* varName,
                                       const char* newValue);

  /**
   * @brief Subscribe to event notifications for the selected renderer
   * @param timeoutSeconds Subscription timeout in seconds (suggested default:
   * 60)
   * @param cb Optional callback to invoke for incoming notifications. If
   *           nullptr the helper's default `processNotification` will be used.
   */
  bool subscribeNotifications(NotificationCallback cb = nullptr,
                              int timeoutSeconds = 3600) {
    p_mgr->setReference(this);
    p_mgr->setSubscribeInterval(timeoutSeconds);
    // register provided callback or fallback to the default processNotification
    p_mgr->onNotification(cb != nullptr ? cb : processNotification);
    return p_mgr->subscribeNotifications();
  }

  /**
   * @brief Set the InstanceID used for AVTransport and RenderingControl actions
   * @param id C-string InstanceID (default is "0")
   */
  void setInstanceID(const char* id) { instance_id = id; }

  /**
   * @brief Set the media URI on the renderer
   * @param uri The media URI to play on the renderer
   * @return true on success, false on error
   */
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
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    return (bool)last_reply;
  }

  /**
   * @brief Start playback on the renderer
   * @return true on success, false on error
   */
  bool play() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) {
      DlnaLogger.log(DlnaLogLevel::Error, "No AVTransport service found");
      return false;
    }
    ActionRequest act(svc, "Play");
    act.addArgument("InstanceID", instance_id);
    act.addArgument("Speed", "1");
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (last_reply) setActiveState(true);
    return (bool)last_reply;
  }

  /**
   * @brief Set the media URI and start playback
   * @param uri URI to play
   * @return true on success, false on error
   */
  bool play(const char* uri) {
    if (!setMediaURI(uri)) return false;
    return play();
  }

  /**
   * @brief Pause playback
   * @return true on success, false on error
   */
  bool pause() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return false;
    ActionRequest act(svc, "Pause");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (last_reply) setActiveState(false);
    return (bool)last_reply;
  }

  /**
   * @brief Stop playback
   * @return true on success, false on error
   */
  bool stop() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return false;
    ActionRequest act(svc, "Stop");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (last_reply) setActiveState(false);
    return (bool)last_reply;
  }

  /**
   * @brief Set renderer volume
   * @param volumePercent Desired volume 0..100
   * @return true on success, false on error
   */
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
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    return (bool)last_reply;
  }

  /**
   * @brief Set mute state on the renderer
   * @param mute true to mute, false to unmute
   * @return true on success, false on error
   */
  bool setMute(bool mute) {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:RenderingControl");
    if (!svc) return false;
    ActionRequest act(svc, "SetMute");
    act.addArgument("InstanceID", "0");
    act.addArgument("Channel", "Master");
    act.addArgument("DesiredMute", mute ? "1" : "0");
    p_mgr->addAction(act);
    ActionReply r = p_mgr->executeActions();
    return (bool)r;
  }

  /**
   * @brief Query the current volume from the renderer
   * @return current volume 0..100, or -1 on error
   */
  int getVolume() {
    DLNAServiceInfo& svc =
        selectService("urn:upnp-org:serviceId:RenderingControl");
    selectService("urn:upnp-org:serviceId:RenderingControl");
    if (!svc) return -1;
    ActionRequest act(svc, "GetVolume");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    last_reply = p_mgr->executeActions();
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
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return -1;
    const char* m = findArgument(last_reply, "CurrentMute");
    if (!m) return -1;
    return atoi(m);
  }

  /**
   * @brief Query current playback position (RelTime)
   * @return position in milliseconds, or 0 if unknown/error
   */
  unsigned long getPositionMs() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return 0;
    ActionRequest act(svc, "GetPositionInfo");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return 0;
    const char* rel = findArgument(last_reply, "RelTime");
    if (!rel) return 0;
    return parseTimeToMs(rel);
  }

  /**
   * @brief Query transport state (e.g. STOPPED, PLAYING, PAUSED_PLAYBACK)
   * @return pointer to a string owned by the helper's registry, or nullptr
   *         on error
   */
  const char* getTransportState() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetTransportInfo");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return nullptr;
    return findArgument(last_reply, "CurrentTransportState");
  }

  /**
   * @brief Get the current media URI from the renderer
   * @return pointer to URI string (owned by reply registry) or nullptr
   */
  const char* getCurrentURI() {
    DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return nullptr;
    ActionRequest act(svc, "GetMediaInfo");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
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
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return -1;
    const char* n = findArgument(last_reply, "NrTracks");
    if (!n) return -1;
    return atoi(n);
  }

  /**
   * @brief Get total media duration in milliseconds (from MediaDuration)
   * @return milliseconds or 0 on error/unknown
   */
  unsigned long getMediaDurationMs() {
    DLNAServiceInfo& svc =
        p_mgr->getService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return 0;
    ActionRequest act(svc, "GetMediaInfo");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
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
    DLNAServiceInfo& svc =
        p_mgr->getService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return -1;
    ActionRequest act(svc, "GetPositionInfo");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
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
    DLNAServiceInfo& svc =
        p_mgr->getService("urn:upnp-org:serviceId:AVTransport");
    if (!svc) return 0;
    ActionRequest act(svc, "GetPositionInfo");
    act.addArgument("InstanceID", instance_id);
    p_mgr->addAction(act);
    last_reply = p_mgr->executeActions();
    if (!last_reply) return 0;
    const char* td = findArgument(last_reply, "TrackDuration");
    if (!td) return 0;
    return parseTimeToMs(td);
  }

  /**
   * @brief Return last ActionReply (from the most recent synchronous call)
   * @return reference to the last ActionReply
   */
  const ActionReply& getLastReply() const { return last_reply; }

  /**
   * @brief Query if the helper considers the renderer to be actively playing
   * @return true if playing, false otherwise
   */
  bool isActive() { return is_active; }

  /**
   * @brief Attach an opaque reference object that will be passed to callbacks
   * @param ref Opaque pointer provided by the caller
   */
  void setReference(void* ref) { reference = ref; }

  /**
   * @brief Register a callback invoked when the active (playing) state changes
   * @param cb Function taking (bool active, void* reference). 'reference' is
   *           the pointer set via setReference().
   */
  void onActiveChanged(std::function<void(bool, void*)> cb) {
    activeChangedCallback = cb;
  }

 protected:
  DLNAControlPoint* p_mgr = nullptr;
  bool is_active = false;
  std::function<void(bool, void*)> activeChangedCallback = nullptr;
  int device_index = 0;
  const char* instance_id = "0";
  const char* device_type_filter_default =
      "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* device_type_filter = device_type_filter_default;
  ActionReply last_reply;
  void* reference = nullptr;
  // Optional transport references (must be set before calling begin())
  DLNAHttpRequest* p_http = nullptr;
  IUDPService* p_udp = nullptr;

  // Helper to update is_active and notify callback only on change
  void setActiveState(bool s) {
    if (is_active == s) return;
    is_active = s;
    DlnaLogger.log(DlnaLogLevel::Info, "ControlPointMediaRenderer active=%s",
                   is_active ? "true" : "false");
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

    DLNAControlPointMediaRenderer* renderer =
        static_cast<DLNAControlPointMediaRenderer*>(reference);
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

  /// Select service by id
  DLNAServiceInfo& selectService(const char* id) {
    // use selected device
    DLNAServiceInfo& s = p_mgr->getDevice().getService(id);
    if (s) return s;

    // fallback: global search
    return p_mgr->getService(id);
  }
};

}  // namespace tiny_dlna
