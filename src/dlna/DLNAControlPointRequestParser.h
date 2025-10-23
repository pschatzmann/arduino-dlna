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
    DlnaLogger.log(DlnaLogLevel::Debug, "Raw UDP packet: %s", req.data.c_str());
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
    // Accept different common header capitalizations
    parse(req.data, "Location:", result->location);
    parse(req.data, "USN:", result->usn);
    parse(req.data, "ST:", result->search_target);
    // Debug: log parsed fields to help diagnose missing Location/ST
    DlnaLogger.log(DlnaLogLevel::Info,
                   "parseMSearchReply parsed -> LOCATION='%s' USN='%s' ST='%s'",
                   result->location.c_str(), result->usn.c_str(),
                   result->search_target.c_str());
    return result;
  }

  NotifyReplyCP* parseNotifyReply(RequestData& req) {
    NotifyReplyCP* result = new NotifyReplyCP();
    parse(req.data, "NOTIFY:", result->delivery_path);
    parse(req.data, "NTS:", result->nts);
    parse(req.data, "NT:", result->search_target);
    parse(req.data, "Location:", result->location);
    parse(req.data, "USN:", result->usn);
    parse(req.data, "Host:", result->delivery_host_and_port);
    parse(req.data, "SID:", result->subscription_id);
    parse(req.data, "SEQ:", result->event_key);
    parse(req.data, "<e:propertyset", result->xml, "</e:propertyset>");
    return result;
  }

  // Case-insensitive parse: searches for tag regardless of capitalization
  bool parse(Str& in, const char* tag, Str& result, const char* end = "\r\n") {
    result.clearAll();
    // create lower-case copies for case-insensitive search using std::string
    Str sin = in.c_str();
    Str stag = tag;
    sin.toLowerCase();
    stag.toLowerCase();

    int start_pos = sin.indexOf(stag.c_str());
    if (start_pos >= 0) {
      // find end position in the original buffer to preserve casing
      int end_pos = sin.indexOf(end, start_pos);
      start_pos += strlen(tag);
      if (end_pos < 0) end_pos = in.indexOf("\n", start_pos);
      if (end_pos >= 0) {
        result.substrView(in.c_str(), start_pos, end_pos);
        //DlnaLogger.log(DlnaLogLevel::Debug, "%s substrView (%d,%d)->%s", tag, start_pos, end_pos, result.c_str());
        result.trim();
        return true;
      }
    }
    return false;
  }
};

}  // namespace tiny_dlna
