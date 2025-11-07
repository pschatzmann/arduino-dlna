#pragma once
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "basic/Logger.h"

#ifdef IS_DESKTOP

namespace tiny_dlna {

/**
 * @brief Tracks dynamic memory allocations and deallocations by class type for
 * debugging and leak detection.
 *
 * AllocationTracker provides a singleton interface to monitor object
 * allocations and frees by class name. It maintains a count of active
 * allocations for each class type and can report leaks and allocation
 * statistics.
 *
 * Usage:
 *   - Use trackAlloc<T>() when allocating an object of type T.
 *   - Use trackFree<T>() when freeing an object of type T.
 *   - Call reportClassCounts() to print the current allocation counts for all
 * tracked classes.
 *   - Call reportLeaks() to print a summary of classes with nonzero allocation
 * counts (potential leaks).
 *
 * Example:
 *   AllocationTracker::getInstance().trackAlloc<MyClass>();
 *   AllocationTracker::getInstance().trackFree<MyClass>();
 */
class AllocationTracker {
 public:
  static AllocationTracker& getInstance() {
    static AllocationTracker instance;
    return instance;
  }

  // Creates a snapshot of the current allocation counts per class name
  void createSnapshot() { snapshot_alloc_count = class_alloc_count; }

  template <typename T>
  void trackAlloc() {
    std::string name = typeid(T).name();
    class_alloc_count[name]++;
    tiny_dlna::DlnaLogger.log(tiny_dlna::DlnaLogLevel::Info,
                              "Allocated instance of %s, count=%d",
                              name.c_str(), class_alloc_count[name]);
  }

  template <typename T>
  void trackFree() {
    std::string name = typeid(T).name();
    auto it = class_alloc_count.find(name);
    if (it != class_alloc_count.end() && it->second > 0) {
      it->second--;
      tiny_dlna::DlnaLogger.log(tiny_dlna::DlnaLogLevel::Info,
                                "Freed instance of %s, count=%d", name.c_str(),
                                it->second);
    } else {
      tiny_dlna::DlnaLogger.log(
          tiny_dlna::DlnaLogLevel::Warning,
          "Attempt to free instance of %s but count is already zero!",
          name.c_str());
    }
  }

  void reportClassCounts() {
  if (class_alloc_count.empty()) return;
    tiny_dlna::DlnaLogger.log(tiny_dlna::DlnaLogLevel::Info,
                              "=== CLASS ALLOCATION COUNTS ===");
    for (const auto& [name, count] : class_alloc_count) {
      tiny_dlna::DlnaLogger.log(tiny_dlna::DlnaLogLevel::Info, "%s: %d",
                                name.c_str(), count);
    }
  }

  bool reportLeaks() {
    if (class_alloc_count.empty()) return false;
    bool any_leak = false;
    // Check for leaks after subtracting snapshot if available
    for (const auto& [name, count] : class_alloc_count) {
      int snapshot_count = 0;
      auto snap_it = snapshot_alloc_count.find(name);
      if (snap_it != snapshot_alloc_count.end()) {
        snapshot_count = snap_it->second;
      }
      int leak_count = count - snapshot_count;
      if (leak_count > 0) {
        any_leak = true;
        break;
      }
    }
    if (!any_leak) {
      tiny_dlna::DlnaLogger.log(tiny_dlna::DlnaLogLevel::Info,
                                "No memory leaks detected!");
    } else {
      tiny_dlna::DlnaLogger.log(tiny_dlna::DlnaLogLevel::Warning,
                                "=== MEMORY LEAK REPORT ===");
      // Print detailed info: total count, snapshot (baseline), and leaked delta
      for (const auto& [name, count] : class_alloc_count) {
        int snapshot_count = 0;
        auto snap_it = snapshot_alloc_count.find(name);
        if (snap_it != snapshot_alloc_count.end()) {
          snapshot_count = snap_it->second;
        }
        int leak_count = count - snapshot_count;
        if (leak_count > 0) {
          tiny_dlna::DlnaLogger.log(
              tiny_dlna::DlnaLogLevel::Warning,
              "Leaked instances: %s: total=%d snapshot=%d leaked=%d",
              name.c_str(), count, snapshot_count, leak_count);
        }
      }
    }
    return any_leak;
  }

 protected:
  // Stores the current allocation counts per class name
  std::map<std::string, int> class_alloc_count;
  // Stores a snapshot of allocation counts per class name
  std::map<std::string, int> snapshot_alloc_count;
  static AllocationTracker* instance;

  AllocationTracker() = default;
};

}  // namespace tiny_dlna

#endif  // IS_DESKTOP