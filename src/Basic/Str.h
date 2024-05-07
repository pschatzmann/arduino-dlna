#pragma once

#include "StrView.h"

namespace tiny_dlna {

/**
 * @brief String implementation which keeps the data on the heap. We grow the
 * allocated memory only if the copy source is not fitting.
 *
 * While it should be avoided to use a lot of heap allocatioins in
 * embedded devices it is sometimes more convinent to allocate a string
 * once on the heap and have the insurance that it might grow
 * if we need to process an unexpected size.
 *
 * We also need to use this if we want to manage a vecor of strings.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Str : public StrView {
 public:
  Str() = default;

  Str(int initialAllocatedLength) : StrView() {
    maxlen = initialAllocatedLength;
  }

  Str(StrView& source) : StrView() { set(source); }

  Str(Str& source) : StrView() { set(source); }

  Str(const char* str) : StrView() {
    if (str != nullptr) {
      len = strlen(str);
      maxlen = len;
      grow(maxlen);
      if (chars != nullptr) {
        strcpy(chars, str);
      }
    }
  }

  // move constructor
  Str(Str&& obj) = default;

  // copy assignment
  Str& operator=(Str&& obj) = default;

  // move assingment
  Str& operator=(Str& obj) {
    set(obj.c_str());
    return *this;
  };

  ~Str() { clear(); }

  void clear() {
    if (chars != nullptr) {
      delete[] chars;
      chars = nullptr;
    }
    len = 0;
  }

  bool isOnHeap() { return true; }

  bool isConst() { return false; }

  void operator=(const char* str) { set(str); }

  void operator=(char* str) { set(str); }

  void operator=(int v) { set(v); }

  void operator=(double v) { set(v); }

  size_t capacity() { return maxlen; }

  void setCapacity(size_t newLen) { grow(newLen); }

  // make sure that the max size is allocated
  void allocate(int len = -1) {
    int new_size = len < 0 ? maxlen : len;
    grow(new_size);
    this->len = new_size;
  }

 protected:
  bool grow(int newMaxLen) {
    bool grown = false;

    if (chars == nullptr || newMaxLen > maxlen) {
      grown = true;
      // we use at minimum the defined maxlen
      int newSize = newMaxLen > maxlen ? newMaxLen : maxlen;
      if (chars != nullptr) {
        char* tmp = chars;
        chars = new char[newSize + 1];
        if (chars != nullptr) {
          strcpy(chars, tmp);
        }
        delete[] tmp;
      } else {
        chars = new char[newSize + 1];
        if (chars != nullptr) chars[0] = 0;
      }
      maxlen = newSize;
    }
    return grown;
  }
};

}  // namespace tiny_dlna
