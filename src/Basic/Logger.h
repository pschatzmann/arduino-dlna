#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
//#include "Platform/AltStream.h"
#include "Arduino.h"

namespace tiny_dlna {

/**
 * @brief Supported log levels
 *
 */
enum DlnaLogLevel { DlnaDebug, DlnaInfo, DlnaWarning, DlnaError };

static const char* HttpDlnaLogLevelStr[] = {"DlnaDebug", "DlnaInfo", "DlnaWarning", "DlnaError"};

/**
 * @brief Logger that writes messages dependent on the log level
 *
 */

class LoggerClass {
 public:
  LoggerClass() = default;

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
      char log_buffer[200];
      strcpy(log_buffer, "DLNA - ");
      strcat(log_buffer, HttpDlnaLogLevelStr[current_level]);
      strcat(log_buffer, ":     ");
      va_list arg;
      va_start(arg, fmt);
      vsprintf(log_buffer + 15, fmt, arg);
      va_end(arg);
      log_stream_ptr->println(log_buffer);
    }
  }

 protected:
  Print* log_stream_ptr = &Serial;
  DlnaLogLevel log_level = DlnaWarning;
};

static LoggerClass DlnaLogger;

}  // namespace tiny_dlna
