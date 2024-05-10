#pragma once

#include "Arduino.h"  // for millis
#include "Schedule.h"
#include "DLNADeviceInfo.h"
#include "UDPService.h"

namespace tiny_dlna {

/**
 * @brief Scheduler which processes all due Schedules (to send out UDP replies)
 * @author Phil Schatzmann
 */

class Scheduler {
 public:
  /// Add a schedule to the scheduler
  void add(Schedule schedule) { queue.push_back(schedule); }

  /// Execute all due schedules
  void execute(UDPService &udp, DLNADeviceInfo &device) {
    bool is_cleanup = false;
    for (auto &s : queue) {
      if (s.time >= millis()) {
        // handle end time: if expired set inactive
        if (s.end_time > millis()) {
          s.active = false;
        }
        // process active schedules
        if (s.active) {
          s.process(udp, device);
          // reschedule if necessary
          if (s.repeat_ms > 0) {
            s.time = millis() + s.repeat_ms;
          } else {
            s.active = false;
            is_cleanup = true;
          }
        }
      }
    }

    if (is_cleanup) cleanup();
  }

 protected:
  List<Schedule> queue;

  void cleanup() {
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      if (!(*it).active){
        DlnaLogger.log(DlnaDebug, "cleanup queue");
        queue.erase(it);
        break;
      }
    }
  }
};

}  // namespace tiny_dlna