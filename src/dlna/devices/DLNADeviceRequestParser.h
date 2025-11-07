#pragma once

#include "udp/IUDPService.h"
#include "dlna/Scheduler.h"
#include "dlna/clients/DLNAControlPoint.h"

namespace tiny_dlna {

/**
 * @brief Translates DLNA UDP Requests to Schedule so that we
 * can schedule a reply
 * @author Phil Schatzmann
 */

class DLNADeviceRequestParser {
 public:
  // add ST that we consider as valid for the actual device
  void addMSearchST(const char* accept) { mx_vector.push_back(accept); }

  Schedule* parse(DLNADeviceInfo& device, RequestData& req) {
    p_device = &device;
    Schedule result;
    if (req.data.contains("M-SEARCH")) {
      return processMSearch(req);
    }

    // We ignore alive notifications
    if (req.data.contains("NOTIFY")) {
      if (req.data.contains("ssdp:alive")) {
        DlnaLogger.log(DlnaLogLevel::Debug, "invalid request: %s",
                       req.data.c_str());
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning, "invalid request: %s",
                       req.data.c_str());
      }
      return nullptr;
    }

    // We currently
    DlnaLogger.log(DlnaLogLevel::Warning, "invalid request: %s",
                   req.data.c_str());

    return nullptr;
  }

 protected:
  Vector<const char*> mx_vector;
  DLNADeviceInfo* p_device = nullptr;

  Schedule* processMSearch(RequestData& req) {
    assert(p_device != nullptr);
    // allocate schedule on the heap; if not relevant we must delete it to avoid
    // leaking the schedule and its internal buffers (e.g. Str)
    MSearchReplySchedule* resultPtr =
        new MSearchReplySchedule{*p_device, req.peer};
    MSearchReplySchedule& result = *resultPtr;
    char tmp[200];
    StrView tmp_str(tmp, 200);

    DlnaLogger.log(DlnaLogLevel::Debug, "Parsing MSSearch");

    // determine MX (seconds to delay response)
    if (parse(req.data, "\nMX:", tmp_str)) {
      result.mx = tmp_str.toInt();
    } else {
      result.mx = 1;
    }
    result.time = millis() + random(result.mx * 1000);

    if (parse(req.data, "\nST:", tmp_str)) {
      result.search_target = tmp;

      // determine ST if relevant for us
      for (auto mx : mx_vector) {
        if (result.search_target.equals(mx)) {
          DlnaLogger.log(DlnaLogLevel::Debug, "- MX: %s -> relevant", mx);
          result.active = true;
        }
      }
      if (!result.active) {
        DlnaLogger.log(DlnaLogLevel::Debug, "-> MX: %s not relevant", tmp);
      }

    } else {
      DlnaLogger.log(DlnaLogLevel::Error, "-> ST: not found");
    }

    // Check if the schedule is valid (e.g., netmask filtering)
    if (result.active && !result.isValid()) {
      result.active = false;
    }

    if (result.active) {
      return resultPtr;
    } else {
      // not relevant -> free allocated schedule
      delete resultPtr;
      return nullptr;
    }
  }

  bool parse(Str& in, const char* tag, StrView& result) {
    result.clearAll();
    int start = in.indexOf(tag);
    if (start >= 0) {
      start += strlen(tag);
      int end = in.indexOf("\r\n", start);
      if (end < 0) end = in.indexOf("\n", start);
      if (end >= 0) {
        result.substrView(in.c_str(), start, end);
        DlnaLogger.log(DlnaLogLevel::Debug, "%s substrView (%d,%d)->%s", tag,
                       start, end, result.c_str());

        result.trim();
        return true;
      }
    }
    return false;
  }
};

}  // namespace tiny_dlna
