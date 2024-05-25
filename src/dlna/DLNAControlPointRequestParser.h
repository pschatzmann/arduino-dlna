#pragma once

#include "IUDPService.h"
#include "Schedule.h"
#include "Scheduler.h"

namespace tiny_dlna {

/**
 * @brief Translates DLNA UDP Requests to Schedule so that we
 * can schedule a reply
 * @author Phil Schatzmann
 */

class DLNAControlPointRequestParser {
 public:
  Schedule* parse(RequestData& req) {
    if (req.data.startsWith("NOTIFY")) {
      return parseNotifyReply(req);
    } else if (req.data.startsWith("HTTP/1.1 200 OK")) {
      return parseMSearchReply(req);
    }
    return nullptr;
  }

 protected:
  MSearchReplyCP* parseMSearchReply(RequestData& req) {
    MSearchReplyCP* result = new MSearchReplyCP();
    parse(req.data, "LOCATION:", result->location);
    parse(req.data, "USN:", result->usn);
    parse(req.data, "ST:", result->search_target);
    return result;
  }

  NotifyReplyCP* parseNotifyReply(RequestData& req) {
    NotifyReplyCP* result = new NotifyReplyCP();
    parse(req.data, "NOTIFY:", result->delivery_path);
    parse(req.data, "HOST:", result->delivery_host_and_port);
    parse(req.data, "SID:", result->subscription_id);
    parse(req.data, "SEQ:", result->event_key);
    parse(req.data, "<e:propertyset", result->xml, "</e:propertyset>");
    return result;
  }
  
  bool parse(Str& in, const char* tag, Str& result, const char* end = "\r\n") {
    result.clearAll();
    int start = in.indexOf(tag);
    if (start >= 0) {
      start += strlen(tag);
      int end = in.indexOf(end, start);
      if (end < 0) end = in.indexOf("\n", start);
      if (end >= 0) {
        result.substring(in.c_str(), start, end);
        DlnaLogger.log(DlnaDebug, "%s substring (%d,%d)->%s", tag, start, end,
                       result.c_str());

        result.trim();
        return true;
      }
    }
    return false;
  }
};

}  // namespace tiny_dlna
