#pragma once

#include "Arduino.h"  // for millis
#include "dlna/DLNADeviceInfo.h"
#include "dlna/udp/IUDPService.h"
#include "dlna/Schedule.h"

namespace tiny_dlna {

/**
 * @brief Scheduler which processes all due Schedules (to send out UDP replies)
 * @author Phil Schatzmann
 */

class Scheduler {
 public:
  /// Add a schedule to the scheduler
  void add(Schedule* schedule) {
    schedule->active = true;
    if (schedule->report_ip) {
      DlnaLogger.log(DlnaLogLevel::Info, "Schedule %s from %s",
                     schedule->name(), schedule->address.toString());
    } else {
      DlnaLogger.log(DlnaLogLevel::Info, "Schedule %s", schedule->name());
    }
    queue.push_back(schedule);
  }

  /// Execute all due schedules
  void execute(IUDPService& udp) {
    // DlnaLogger.log(DlnaLogLevel::Debug, "Scheduler::execute");
    bool is_cleanup = false;
    for (auto& p_s : queue) {
      if (p_s == nullptr) continue;
      Schedule& s = *p_s;
      if (millis() >= s.time) {
        // handle end time: if expired set inactive
        if (s.end_time != 0ul && millis() > s.end_time) {
          s.active = false;
        }
        // process active schedules
        if (s.active) {
          DlnaLogger.log(DlnaLogLevel::Debug,
                         "Scheduler::execute %s: Executing", s.name());

          s.process(udp);
          // reschedule if necessary
          if (s.repeat_ms > 0) {
            s.time = millis() + s.repeat_ms;
          } else {
            s.active = false;
            is_cleanup = true;
          }
        } else {
          DlnaLogger.log(DlnaLogLevel::Debug, "Scheduler::execute %s: Inactive",
                         s.name());
          is_cleanup = true;
        }
      }
    }

    cleanup();
  }

  /// Returns true if there is any active schedule with name "MSearch"
  bool isMSearchActive() {
    for (auto& p_s : queue) {
      if (p_s == nullptr) continue;
      Schedule& s = *p_s;
      if (s.active && StrView(s.name()).equals("MSearch")) return true;
    }
    return false;
  }

  /// Number of queued schedules
  int size() { return queue.size(); }

  void setActive(bool flag) { is_active = flag; }

  bool isActive() { return is_active; }

 protected:
  Vector<Schedule*> queue;
  bool is_active = true;

  void cleanup() {
    DlnaLogger.log(DlnaLogLevel::Debug, "Scheduler::cleanup: for %d items",
                   queue.size());
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      auto p_rule = *it;
      if (p_rule == nullptr) continue;
      if (!(p_rule)->active) {
        DlnaLogger.log(DlnaLogLevel::Debug, "Scheduler::cleanup queue: %s",
                       p_rule->name());
        // remove schedule from collection using iterator overload
        queue.erase(it);
        // delete schedule
        delete p_rule;
        break;
      }
    }
  }
};

}  // namespace tiny_dlna