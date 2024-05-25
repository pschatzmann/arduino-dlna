#pragma once
#include "basic/Str.h"

namespace tiny_dlna {

class StringRegistry {
 public:
  const char* add(char* in) {
    for (auto str : strings) {
      if (str.equals(in)) {
        return str.c_str();
      }
    }
    strings.push_back(in);
    return strings[strings.size() - 1].c_str();
  }

  void clear() { strings.clear(); }

 protected:
  Vector<Str> strings;
};

}  // namespace tiny_dlna