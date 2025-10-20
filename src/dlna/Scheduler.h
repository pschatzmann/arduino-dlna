#pragma once

#include "Arduino.h"  // for millis
#include "DLNADeviceInfo.h"
#include "IUDPService.h"
#include "Schedule.h"

namespace tiny_dlna {

/**
 * @brief Scheduler which processes all due Schedules (to send out UDP replies)
 * @author Phil Schatzmann
 */

class Scheduler {
 public:
  /// Add a schedule to the scheduler
  void add(Schedule *schedule) {
    schedule->active = true;
    DlnaLogger.log(DlnaLogLevel::Info, "Schedule %s", schedule->name());
    queue.push_back(schedule);
  }

  /// Execute all due schedules
  void execute(IUDPService &udp) {
    // DlnaLogger.log(DlnaLogLevel::Debug, "Scheduler::execute");
    bool is_cleanup = false;
    for (auto &p_s : queue) {
      if (p_s == nullptr) continue;
      Schedule &s = *p_s;
      if (millis() >= s.time) {
        // handle end time: if expired set inactive
        if (s.end_time != 0ul && millis() > s.end_time) {
          s.active = false;
        }
        // process active schedules
        if (s.active) {
          DlnaLogger.log(DlnaLogLevel::Debug, "Executing", s.name());

          s.process(udp);
          // reschedule if necessary
          if (s.repeat_ms > 0) {
            s.time = millis() + s.repeat_ms;
          } else {
            s.active = false;
            is_cleanup = true;
          }
        } else {
          DlnaLogger.log(DlnaLogLevel::Debug, "Inactive", s.name());
        }
      }
    }

    if (is_cleanup) cleanup();
  }

 protected:
  Vector<Schedule *> queue;

  void cleanup() {
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      auto p_rule = *it;
      if (!(p_rule)->active) {
        DlnaLogger.log(DlnaLogLevel::Debug, "cleanup queue: %s", p_rule->name());
        // remove schedule from collection
        queue.erase(it);
        // delete schedule
        delete p_rule;
        break;
      }
    }
  }
};

}  // namespace tiny_dlna