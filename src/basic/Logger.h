#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "Arduino.h"

#define DLNA_MAX_LOG_SIZE 400

namespace tiny_dlna {

/**
 * @brief Supported log levels
 *
 */
enum DlnaLogLevel { DlnaDebug, DlnaInfo, DlnaWarning, DlnaError };

static const char* HttpDlnaLogLevelStr[] = {"Debug", "Info", "Warning", "Error"};

/**
 * @brief Logger that writes messages dependent on the log level
 *
 */

class LoggerClass {
 public:

  // activate the logging
  virtual void begin(Print& out, DlnaLogLevel level = DlnaError) {
    this->log_stream_ptr = &out;
    this->log_level = level;
  }

  void setLevel(DlnaLogLevel l) { log_level = l; }

  // checks if the logging is active
  virtual bool isLogging() { return log_stream_ptr != nullptr; }

  /// Print log message
  void log(DlnaLogLevel current_level, const char* fmt...) {
    if (current_level >= log_level && log_stream_ptr != nullptr &&
        fmt != nullptr) {
      char log_buffer[DLNA_MAX_LOG_SIZE];
      strcpy(log_buffer, "DLNA - ");
      strcat(log_buffer, HttpDlnaLogLevelStr[current_level]);
      strcat(log_buffer, ":     ");
      va_list arg;
      va_start(arg, fmt);
      vsnprintf(log_buffer + 15, DLNA_MAX_LOG_SIZE - 15, fmt, arg);
      va_end(arg);
      log_stream_ptr->println(log_buffer);
    }
  }

 protected:
  Print* log_stream_ptr = &Serial;
  DlnaLogLevel log_level = DlnaWarning;
};

extern LoggerClass DlnaLogger;

}  // namespace tiny_dlna
