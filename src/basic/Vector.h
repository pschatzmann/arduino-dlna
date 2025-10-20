#pragma once
#ifdef USE_INITIALIZER_LIST
#include "InitializerList.h"
#endif
#include <assert.h>

#include "Allocator.h"

namespace tiny_dlna {

/**
 * @brief Vector implementation which provides the most important methods as
 *defined by std::vector. This class it is quite handy to have and most of the
 *times quite better then dealing with raw c arrays.
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/

template <class T>
class Vector {
 public:

  /**
   * @brief Lightweight iterator for `tiny_dlna::Vector<T>`.
   *
   * Provides a minimal iterator interface (increment, decrement, dereference,
   * comparison) suitable for range-style loops and manual iteration. This
   * iterator holds a raw pointer to the element storage and a position index
   * (`pos_`). Note that it is a simple forward/backward iterator and does
   * not provide full random-access iterator guarantees beyond `operator+`.
   */
  class Iterator {
   protected:
    T *ptr;
    size_t pos_;

   public:
    Iterator() {}
    Iterator(T *parPtr, size_t pos) {
      this->ptr = parPtr;
      this->pos_ = pos;
    }
    // copy constructor
    Iterator(const Iterator &copyFrom) {
      ptr = copyFrom.ptr;
      pos_ = copyFrom.pos_;
    }
    Iterator operator++(int n) {
      ptr++;
      pos_++;
      return *this;
    }
    Iterator operator++() {
      ptr++;
      pos_++;
      return *this;
    }
    Iterator operator--(int n) {
      ptr--;
      pos_--;
      return *this;
    }
    Iterator operator--() {
      ptr--;
      pos_--;
      return *this;
    }
    Iterator operator+(int offset) {
      pos_ += offset;
      return Iterator(ptr + offset, offset);
    }
    bool operator==(Iterator it) { return ptr == it.getPtr(); }
    bool operator<(Iterator it) { return ptr < it.getPtr(); }
    bool operator<=(Iterator it) { return ptr <= it.getPtr(); }
    bool operator>(Iterator it) { return ptr > it.getPtr(); }
    bool operator>=(Iterator it) { return ptr >= it.getPtr(); }
    bool operator!=(Iterator it) { return ptr != it.getPtr(); }
    T &operator*() { return *ptr; }
    T *operator->() { return ptr; }
    T *getPtr() { return ptr; }
    size_t pos() { return pos_; }
    size_t operator-(Iterator it) { return (ptr - it.getPtr()); }
  };

#ifdef USE_INITIALIZER_LIST

  /// support for initializer_list
  Vector(std::initializer_list<T> iniList) {
    resize(iniList.size());
    int pos = 0;
    for (auto &obj : iniList) {
      p_data[pos++] = obj;
    }
  }

#endif

  /// Default constructor: size 0 with DefaultAllocator
  Vector(size_t len = 0, Allocator &allocator = DefaultAllocator) {
    setAllocator(allocator);
    resize_internal(len, false);
  }

  /// Constructor with only allocator
  Vector(Allocator &allocator) { setAllocator(allocator); }

  /// Allocate size and initialize array
  Vector(int size, T value, Allocator &allocator = DefaultAllocator) {
    setAllocator(allocator);
    resize(size);
    for (int j = 0; j < size; j++) {
      p_data[j] = value;
    }
  }

  /// Move constructor
  Vector(Vector<T> &&moveFrom) {
    swap(moveFrom);
    moveFrom.clear();
  };

  /// Move operator
  Vector &operator=(Vector &&moveFrom) {
    swap(moveFrom);
    moveFrom.clear();
    return *this;
  }

  /// copy constructor
  Vector(Vector<T> &copyFrom) {
    this->p_allocator = copyFrom.p_allocator;
    resize_internal(copyFrom.size(), false);
    for (int j = 0; j < copyFrom.size(); j++) {
      p_data[j] = copyFrom[j];
    }
    this->len = copyFrom.size();
  }

  /// copy operator
  Vector<T> &operator=(Vector<T> &copyFrom) {
    resize_internal(copyFrom.size(), false);
    for (int j = 0; j < copyFrom.size(); j++) {
      p_data[j] = copyFrom[j];
    }
    this->len = copyFrom.size();
    return *this;
  }

  /// legacy constructor with pointer range
  Vector(T *from, T *to, Allocator &allocator = DefaultAllocator) {
    this->p_allocator = &allocator;
    this->len = to - from;
    resize_internal(this->len, false);
    for (size_t j = 0; j < this->len; j++) {
      p_data[j] = from[j];
    }
  }

  /// Destructor
  virtual ~Vector() { reset(); }

  void setAllocator(Allocator &allocator) { p_allocator = &allocator; }

  void clear() { len = 0; }

  int size() { return len; }

  bool empty() { return size() == 0; }

  void push_back(T &&value) {
    resize_internal(len + 1, true);
    p_data[len] = value;
    len++;
  }

  void push_back(T &value) {
    resize_internal(len + 1, true);
    p_data[len] = value;
    len++;
  }

  void push_front(T &value) {
    resize_internal(len + 1, true);
    // memmove(p_data,p_data+1,len*sizeof(T));
    for (int j = len; j >= 0; j--) {
      p_data[j + 1] = p_data[j];
    }
    p_data[0] = value;
    len++;
  }

  void push_front(T &&value) {
    resize_internal(len + 1, true);
    // memmove(p_data,p_data+1,len*sizeof(T));
    for (int j = len; j >= 0; j--) {
      p_data[j + 1] = p_data[j];
    }
    p_data[0] = value;
    len++;
  }

  void pop_back() {
    if (len > 0) {
      len--;
    }
  }

  void pop_front() { erase(0); }

  void assign(Iterator v1, Iterator v2) {
    size_t newLen = v2 - v1;
    resize_internal(newLen, false);
    this->len = newLen;
    int pos = 0;
    for (auto ptr = v1; ptr != v2; ptr++) {
      p_data[pos++] = *ptr;
    }
  }

  void assign(size_t number, T value) {
    resize_internal(number, false);
    this->len = number;
    for (int j = 0; j < number; j++) {
      p_data[j] = value;
    }
  }

  void swap(Vector<T> &in) {
    // save data
    T *dataCpy = p_data;
    int bufferLenCpy = max_capacity;
    int lenCpy = len;
    // swap this
    p_data = in.p_data;
    len = in.len;
    max_capacity = in.max_capacity;
    // swp in
    in.p_data = dataCpy;
    in.len = lenCpy;
    in.max_capacity = bufferLenCpy;
  }

  T &operator[](int index) {
    assert(p_data != nullptr);
    return p_data[index];
  }

  T &operator[](const int index) const { return p_data[index]; }

  bool resize(int newSize, T value) {
    if (resize(newSize)) {
      for (int j = 0; j < newSize; j++) {
        p_data[j] = value;
      }
      return true;
    }
    return false;
  }

  void shrink_to_fit() { resize_internal(this->len, true, true); }

  int capacity() { return this->max_capacity; }

  bool resize(int newSize) {
    int oldSize = this->len;
    resize_internal(newSize, true);
    this->len = newSize;
    return this->len != oldSize;
  }

  Iterator begin() { return Iterator(p_data, 0); }

  T &back() { return *Iterator(p_data + (len - 1), len - 1); }

  Iterator end() { return Iterator(p_data + len, len); }

  // removes a single element
  void erase(Iterator it) { return erase(it.pos()); }

  // removes a single element
  void erase(int pos) {
    if (pos < len) {
      int lenToEnd = len - pos - 1;
      // call destructor on data to be erased
      p_data[pos].~T();
      // shift values by 1 position
      memmove((void *)&p_data[pos], (void *)(&p_data[pos + 1]),
              lenToEnd * sizeof(T));

      // make sure that we have a valid object at the end
      // p_data[len - 1] = T();
      new (&(p_data[len - 1])) T();
      len--;
    }
  }

  T *data() { return p_data; }

  operator bool() const { return p_data != nullptr; }

  int indexOf(T obj) {
    for (int j = 0; j < size(); j++) {
      if (p_data[j] == obj) return j;
    }
    return -1;
  }

  bool contains(T obj) { return indexOf(obj) >= 0; }

  void swap(T &other) {
    // save values
    int temp_blen = max_capacity;
    int temp_len = len;
    T *temp_data = p_data;
    // swap from other
    max_capacity = other.bufferLen;
    len = other.len;
    p_data = other.p_data;
    // set other
    other.bufferLen = temp_blen;
    other.len = temp_len;
    other.p_data = temp_data;
  }

  void reset() {
    clear();
    shrink_to_fit();
    deleteArray(p_data, size());  // delete [] this->p_data;
    p_data = nullptr;
  }

 protected:
  int max_capacity = 0;
  int len = 0;
  T *p_data = nullptr;
  Allocator *p_allocator = &DefaultAllocator;

  void resize_internal(int newSize, bool copy, bool shrink = false) {
    if (newSize <= 0) return;
    if (newSize > max_capacity || this->p_data == nullptr || shrink) {
      T *oldData = p_data;
      int oldBufferLen = this->max_capacity;
      p_data = newArray(newSize);  // new T[newSize+1];
      assert(p_data != nullptr);
      this->max_capacity = newSize;
      if (oldData != nullptr) {
        if (copy && this->len > 0) {
          // save existing data
          memmove((void *)p_data, (void *)oldData, len * sizeof(T));
          // clear to prevent double release
          memset((void *)oldData, 0, len * sizeof(T));
        }
        if (shrink) {
          cleanup(oldData, newSize, oldBufferLen);
        }
        deleteArray(oldData, oldBufferLen);  // delete [] oldData;
      }
    }
  }

  T *newArray(int newSize) {
    T *data;
#if USE_ALLOCATOR
    data = p_allocator->createArray<T>(newSize);  // new T[newSize+1];
#else
    data = new T[newSize];
#endif
    return data;
  }

  void deleteArray(T *oldData, int oldBufferLen) {
#if USE_ALLOCATOR
    p_allocator->removeArray(oldData, oldBufferLen);  // delete [] oldData;
#else
    delete[] oldData;
#endif
  }

  void cleanup(T *data, int from, int to) {
    for (int j = from; j < to; j++) {
      data[j].~T();
    }
  }
};

}  // namespace tiny_dlna