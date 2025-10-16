// Header-only control point helper for MediaRenderer devices
#pragma once

#include "dlna/DLNAControlPointMgr.h"
#include "dlna/service/Action.h"

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

	/// Return number of discovered devices matching the renderer filter
	int getDeviceCount() { 
		int count = 0;
		for (auto &d : mgr.getDevices()) {
			const char* dt = d.getDeviceType();
			if (!device_type_filter || StrView(dt).contains(device_type_filter)) count++;
		}
		return count;
	}

	/// Restrict this helper to devices of the given device type (default: MediaRenderer)
	void setDeviceTypeFilter(const char* filter) { device_type_filter = filter; }

	/// Return last ActionReply (from the most recent call)
	const ActionReply& getLastReply() const { return last_reply; }

	/// Set the active renderer by index (optional). Default uses first matching service.
	void setDeviceIndex(int idx) { device_index = idx; }

	/// Set AVTransport URI on the renderer (synchronous)
	bool setAVTransportURI(const char* uri) {
		DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
		if (!svc) {
			DlnaLogger.log(DlnaLogLevel::Error, "No AVTransport service found");
			return false;
		}
		ActionRequest act(svc, "SetAVTransportURI");
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
		act.addArgument("Speed", "1");
		mgr.addAction(act);
		last_reply = mgr.executeActions();
		return (bool)last_reply;
	}

	/// Pause
	bool pause() {
		DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
		if (!svc) return false;
		ActionRequest act(svc, "Pause");
		act.addArgument("InstanceID", "0");
		mgr.addAction(act);
		last_reply = mgr.executeActions();
		return (bool)last_reply;
	}

	/// Stop
	bool stop() {
		DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:AVTransport");
		if (!svc) return false;
		ActionRequest act(svc, "Stop");
		act.addArgument("InstanceID", "0");
		mgr.addAction(act);
		last_reply = mgr.executeActions();
		return (bool)last_reply;
	}

	/// Set volume (0..100)
	bool setVolume(int volumePercent) {
		if (volumePercent < 0) volumePercent = 0;
		if (volumePercent > 100) volumePercent = 100;
		DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:RenderingControl");
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
		DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:RenderingControl");
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
		DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:RenderingControl");
		if (!svc) return -1;
		ActionRequest act(svc, "GetVolume");
		act.addArgument("InstanceID", "0");
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
		DLNAServiceInfo& svc = selectService("urn:upnp-org:serviceId:RenderingControl");
		if (!svc) return -1;
		ActionRequest act(svc, "GetMute");
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
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
		act.addArgument("InstanceID", "0");
		mgr.addAction(act);
		last_reply = mgr.executeActions();
		if (!last_reply) return 0;
		const char* td = findArgument(last_reply, "TrackDuration");
		if (!td) return 0;
		return parseTimeToMs(td);
	}

 protected:
	DLNAControlPointMgr& mgr;
	int device_index = 0;
	const char* device_type_filter = "urn:schemas-upnp-org:device:MediaRenderer:1";

	// stores the most recent action reply
	ActionReply last_reply;

	// Helper: find argument by name in ActionReply (returns nullptr if not found)
	const char* findArgument(ActionReply &r, const char* name) {
		for (auto &a : r.arguments) {
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
		while (*p) { if (*p == ':') colonCount++; p++; }
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
		unsigned long total = (unsigned long)h * 3600UL + (unsigned long)m * 60UL + (unsigned long)s;
		return total * 1000UL;
	}

	// Select service on a device filtered by device_type_filter
	DLNAServiceInfo& selectService(const char* id) {
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
		return mgr.getService(id);
	}
};

}  // namespace tiny_dlna

