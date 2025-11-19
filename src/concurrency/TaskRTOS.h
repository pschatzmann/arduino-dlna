#pragma once

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#elif defined(__linux__)
#else
#include "FreeRTOS.h"
#include "task.h"
#endif
#include <functional>

#include "basic/Logger.h"

namespace tiny_dlna {

/**
 * @brief FreeRTOS task
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 */
class TaskRTOS {
 public:
  /// Defines and creates a FreeRTOS task
  TaskRTOS(const char* name, int stackSize, int priority = 1, int core = -1) {
    create(name, stackSize, priority, core);
  }

  TaskRTOS() {
    create("TaskRTOS", 4096, 1, -1);
  };

  ~TaskRTOS() { remove(); }

  /// deletes the FreeRTOS task
  void remove() {
    if (xHandle != nullptr) {
      suspend();
      vTaskDelete(xHandle);
      xHandle = nullptr;
    }
  }

  bool begin(std::function<void()> process) {
    DlnaLogger.log(DlnaLogLevel::Info, "staring task");
    loop_code = process;
    resume();
    return true;
  }

  /// suspends the task
  void end() { suspend(); }

  void suspend() {
    if (xHandle == nullptr) {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "TaskRTOS::suspend() called but task not created");
      return;
    }
    vTaskSuspend(xHandle);
  }

  void resume() {
    if (xHandle == nullptr) {
      DlnaLogger.log(DlnaLogLevel::Warning,
                     "TaskRTOS::resume() called but task not created");
      return;
    }

    vTaskResume(xHandle);
  }

  TaskHandle_t& getTaskHandle() { return xHandle; }

  void setReference(void* r) { ref = r; }

  void* getReference() { return ref; }

#ifdef ESP32
  int getCoreID() { return xPortGetCoreID(); }
#endif

 protected:
  TaskHandle_t xHandle = nullptr;
  std::function<void()> loop_code = nop;
  void* ref;

  /// If you used the empty constructor, you need to call create!
  bool create(const char* name, int stackSize, int priority = 1,
              int core = -1) {
    if (xHandle != 0) return false;
#ifdef ESP32
    if (core >= 0)
      xTaskCreatePinnedToCore(task_loop, name, stackSize, this, priority,
                              &xHandle, core);
    else
      xTaskCreate(task_loop, name, stackSize, this, priority, &xHandle);
#else
    xTaskCreate(task_loop, name, stackSize, this, priority, &xHandle);
#endif
    delay(50);
    suspend();
    return xHandle != nullptr;
  }

  static void nop() { delay(100); }

  static void task_loop(void* arg) {
    TaskRTOS* self = (TaskRTOS*)arg;
    while (true) {
      self->loop_code();
    }
  }
};

using Task = TaskRTOS;

}  // namespace tiny_dlna