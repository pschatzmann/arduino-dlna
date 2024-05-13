#pragma once
#include "basic/Str.h"
#include "basic/Vector.h"
#include "basic/Logger.h"
#include "Stream.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace tiny_dlna {

/**
 * @brief API for http parameters: key=value&key1=value1
 *
 */
class HttpParameters {
  struct HttpParameterEntry {
    HttpParameterEntry() = default;
    Str key = nullptr;
    Str value = nullptr;
    void clear() {
      key.clear();
      value.clear();
    }
  };

public:
  /// Default Constructor
  HttpParameters(const int maxLen = 256) { max_len = maxLen; };

  /// Destructor
  ~HttpParameters() { clear(); }

  /// Parses the parameters in the client stream
  void parse(Stream &in) {
    char buffer[max_len];
    while (in.available() > 0) {
      memset(buffer, 0, max_len);
      in.readBytesUntil('&', buffer, max_len);
      StrView str(buffer);
      DlnaLogger.log(DlnaInfo, "parameter: %s", buffer);
      urldecode2(buffer, buffer);
      DlnaLogger.log(DlnaInfo, "parameter decoded: %s", buffer);
      int pos = str.indexOf("=");
      if (pos > 0) {
        buffer[pos] = 0; // delimit key
        const char *key = buffer;
        const char *value = buffer + pos + 1;
        DlnaLogger.log(DlnaDebug, "key: %s", key);
        DlnaLogger.log(DlnaDebug, "value: %s", value);
        HttpParameterEntry *entry = getParameter(key);
        if (entry != nullptr) {
          entry->value = value;
        } else {
          entry = new HttpParameterEntry();
          entry->key = key;
          entry->value = value;
          parameters.push_back(entry);
        }
      }
    }
  }

  /// Parses the parameters in the client stream and provides the result via a
  /// callback method
  void parse(Stream &in, void (*callback)(const char *key, const char *value)) {
    char buffer[max_len];
    while (in.available() > 0) {
      memset(buffer, 0, max_len);
      in.readBytesUntil('&', buffer, max_len);
      StrView str(buffer);
      DlnaLogger.log(DlnaInfo, "parameter: %s", buffer);
      urldecode2(buffer, buffer);
      DlnaLogger.log(DlnaInfo, "parameter decoded: %s", buffer);
      int pos = str.indexOf("=");
      if (pos > 0) {
        buffer[pos] = 0; // delimit key
        const char *key = buffer;
        const char *value = buffer + pos + 1;
        callback(key, value);
      }
    }
  }

  /// Checks if the parameter exists
  bool hasKey(const char *key) {
    for (auto &entry : parameters) {
      if (entry->key.equals(key)) {
        return true;
      }
    }
    return false;
  }

  /// Returns a HttpParameterEntry for a parameter id
  HttpParameterEntry *getParameter(const char *key) {
    for (auto &entry : parameters) {
      if (entry->key.equals(key)) {
        return entry;
      }
    }
    return nullptr;
  }

  /// Returns the value for a parameter id as string
  const char *getValue(const char *key) {
    for (auto &entry : parameters) {
      if (entry->key.equals(key)) {
        return entry->value.c_str();
      }
    }
    return nullptr;
  }

  /// Returns the value for a parameter id as float
  float getFloat(const char *key) {
    for (auto &entry : parameters) {
      if (entry->key.equals(key)) {
        return entry->value.toFloat();
      }
    }
    return 0;
  }

  /// Returns the value for a parameter id as int
  int getInt(const char *key) {
    for (auto &entry : parameters) {
      if (entry->key.equals(key)) {
        return entry->value.toInt();
      }
    }
    return 0;
  }

  /// Clears all values
  void clear() {
    for (auto entry : parameters) {
      delete entry;
    }
    parameters.clear();
  }

protected:
  Vector<HttpParameterEntry *> parameters;
  int max_len;

  void urldecode2(char *dst, const char *src) {
    char a, b;
    while (*src) {
      if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
          (isxdigit(a) && isxdigit(b))) {
        if (a >= 'a')
          a -= 'a' - 'A';
        if (a >= 'A')
          a -= ('A' - 10);
        else
          a -= '0';
        if (b >= 'a')
          b -= 'a' - 'A';
        if (b >= 'A')
          b -= ('A' - 10);
        else
          b -= '0';
        *dst++ = 16 * a + b;
        src += 3;
      } else if (*src == '+') {
        *dst++ = ' ';
        src++;
      } else {
        *dst++ = *src++;
      }
    }
    *dst++ = '\0';
  }
};

} // namespace tiny_dlna