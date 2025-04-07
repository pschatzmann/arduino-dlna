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
    } else if (req.data.startsWith("M-SEARCH")) {
      DlnaLogger.log(DlnaLogLevel::Debug, "M-SEARCH request ignored");
    } else {
      DlnaLogger.log(DlnaLogLevel::Info, "Not handled: %s", req.data);
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
    parse(req.data, "NTS:", result->nts);
    parse(req.data, "NT:", result->search_target);
    parse(req.data, "Location:", result->location);
    if (result->location.isEmpty()){
      parse(req.data, "LOCATION:", result->location);
    }
    parse(req.data, "USN:", result->usn);
    parse(req.data, "Host:", result->delivery_host_and_port);
    if (result->delivery_host_and_port.isEmpty()){
      parse(req.data, "HOST:", result->delivery_host_and_port);
    }

    parse(req.data, "SID:", result->subscription_id);
    parse(req.data, "SEQ:", result->event_key);
    parse(req.data, "<e:propertyset", result->xml, "</e:propertyset>");
    return result;
  }

  bool parse(Str& in, const char* tag, Str& result, const char* end = "\r\n") {
    result.clearAll();
    int start_pos = in.indexOf(tag);
    if (start_pos >= 0) {
      int end_pos = in.indexOf(end, start_pos);
      start_pos += strlen(tag);
      if (end_pos < 0) end_pos = in.indexOf("\n", start_pos);
      if (end_pos >= 0) {
        result.substrView(in.c_str(), start_pos, end_pos);
        DlnaLogger.log(DlnaLogLevel::Debug, "%s substrView (%d,%d)->%s", tag, start_pos, end_pos,
                       result.c_str());

        result.trim();
        return true;
      }
    }
    return false;
  }
};

}  // namespace tiny_dlna
