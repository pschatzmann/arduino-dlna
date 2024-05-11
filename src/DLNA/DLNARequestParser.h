#pragma once

#include "Scheduler.h"
#include "UDPService.h"

namespace tiny_dlna {

/**
 * @brief Translates DLNA UDP Requests to Schedule so that we
 * can schedule a reply
 * @author Phil Schatzmann
 */

class DLNARequestParser {
 public:
  Schedule parse(RequestData &req) {
    Schedule result;
    if (req.data.startsWith("M-SEARCH")) {
      return processMSearch(req);
    }

    DlnaLogger.log(DlnaInfo, "invalid request: %s", req.data);
    return result;
  }

 protected:
  Schedule processMSearch(RequestData &req) {
    MSearchReplySchedule result{req.peer};
    // determine MX (seconds to delay response)
    int pos = req.data.indexOf("MX:");
    if (pos >= 0) {
      DlnaLogger.log(DlnaInfo, "processing MSSearch");
      Str mx_str(req.data.c_str() + pos + 3);
      int mx = mx_str.toInt();
      result.active = true;
      result.time = millis() + random(mx * 1000);
      // // determine ST (Search Target)
      // int pos = req.data.indexOf("ST:");
      // if (pos >= 0) {
      //   // ST: upnp:rootdevice
      //   //
      // }
    }

    return result;
  }
};

}  // namespace tiny_dlna
