#pragma once

#include "IUDPService.h"
#include "Scheduler.h"
#include "DLNAControlPointMgr.h"

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
        DlnaLogger.log(DlnaLogLevel::Debug, "invalid request: %s", req.data.c_str());
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning, "invalid request: %s", req.data.c_str());
      }
      return nullptr;
    }

    // We currently
    DlnaLogger.log(DlnaLogLevel::Warning, "invalid request: %s", req.data.c_str());

    return nullptr;
  }

 protected:
  Vector<const char*> mx_vector;
  DLNADeviceInfo* p_device = nullptr;

  Schedule* processMSearch(RequestData& req) {
    assert(p_device != nullptr);
    MSearchReplySchedule& result =
        *new MSearchReplySchedule{*p_device, req.peer};
    char tmp[200];
    StrView tmp_str(tmp, 200);

    DlnaLogger.log(DlnaLogLevel::Info, "Parsing MSSearch");

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
          DlnaLogger.log(DlnaLogLevel::Info, "- MX: %s -> relevant", mx);
          result.active = true;
        }
      }
      if (!result.active) {
        DlnaLogger.log(DlnaLogLevel::Info, "-> MX: %s not relevant", tmp);
      }

    } else {
      DlnaLogger.log(DlnaLogLevel::Error, "-> ST: not found");
    }

    return result.active ? &result : nullptr;
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
        DlnaLogger.log(DlnaLogLevel::Debug, "%s substrView (%d,%d)->%s", tag, start, end,
                       result.c_str());

        result.trim();
        return true;
      }
    }
    return false;
  }
};

}  // namespace tiny_dlna
