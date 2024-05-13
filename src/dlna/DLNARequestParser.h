#pragma once

#include "IUDPService.h"
#include "Scheduler.h"

namespace tiny_dlna {

/**
 * @brief Translates DLNA UDP Requests to Schedule so that we
 * can schedule a reply
 * @author Phil Schatzmann
 */

class DLNARequestParser {
 public:
  // add ST that we consider as valid for the actual device
  void addMSearchST(const char* accept) { mx_vector.push_back(accept); }

  Schedule* parse(RequestData& req) {
    Schedule result;
    if (req.data.contains("M-SEARCH")) {
      return processMSearch(req);
    }

    if (req.data.contains("SUBSCRIBE") || req.data.contains("NOTIFY") ||
        req.data.contains("SEARCH")) {
      DlnaLogger.log(DlnaInfo, "invalid request: %s", req.data);
    } else {
      DlnaLogger.log(DlnaDebug, "invalid request: %s", req.data);
    }

    return nullptr;
  }

 protected:
  Vector<const char*> mx_vector;

  Schedule* processMSearch(RequestData& req) {
    MSearchReplySchedule& result = *new MSearchReplySchedule{req.peer};
    char tmp[200];
    StrView tmp_str(tmp, 200);

    DlnaLogger.log(DlnaInfo, "Parsing MSSearch");

    // determine MX (seconds to delay response)
    if (parse(req.data, "\nMX:", tmp_str)) {
      result.mx = tmp_str.toInt();
    }
    result.time = millis() + random(result.mx * 1000);

    if (parse(req.data, "\nST:", tmp_str)) {
      result.search_target = tmp;

      // determine ST if relevant for us
      for (auto mx : mx_vector) {
        if (result.search_target.equals(mx)) {
          DlnaLogger.log(DlnaInfo, "MX: %s -> relevant", mx);
          result.active = true;
        }
      }
      if (!result.active) {
        DlnaLogger.log(DlnaInfo, "MX: %s not relevant", tmp);
      }

    } else {
      DlnaLogger.log(DlnaInfo, "ST: not found");
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
        result.substring(in.c_str(), start, end);
        DlnaLogger.log(DlnaDebug, "%s substring (%d,%d)->%s",tag, start, end, result.c_str());

        result.trim();
        return true;
      }
    }
    return false;
  }
};

}  // namespace tiny_dlna