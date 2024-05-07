#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Platform/AltStream.h"

namespace tiny_dlna {

void stop() {
  while (true) delay(1000);
}

/**
 * @brief Supported log levels
 *
 */
enum LogLevel { Debug, Info, Warning, Error };

const char* HttpLogLevelStr[] = {"Debug", "Info", "Warning", "Error"};

/**
 * @brief Logger that writes messages dependent on the log level
 *
 */

class LoggerClass {
 public:
  LoggerClass() = default;

  // activate the logging
  virtual void begin(Stream& out, LogLevel level = Error) {
    this->log_stream_ptr = &out;
    this->log_level = level;
  }

  void setLevel(LogLevel l) { log_level = l; }

  // checks if the logging is active
  virtual bool isLogging() { return log_stream_ptr != nullptr; }

  /// Print log message
  void log(LogLevel current_level, const char* fmt...) {
    if (current_level >= log_level && log_stream_ptr != nullptr &&
        fmt != nullptr) {
      char log_buffer[200];
      strcpy(log_buffer, "TinyHttp - ");
      strcat(log_buffer, HttpLogLevelStr[current_level]);
      strcat(log_buffer, ":     ");
      va_list arg;
      va_start(arg, fmt);
      vsprintf(log_buffer + 9, fmt, arg);
      va_end(arg);
      log_stream_ptr->println(log_buffer);
    }
  }

 protected:
  Stream* log_stream_ptr = &Serial;
  LogLevel log_level = Warning;
};

LoggerClass Logger;

}  // namespace tiny_dlna
