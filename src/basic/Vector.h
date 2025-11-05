#include "dlna_config.h"

#pragma once
#include <assert.h>
#include "Allocator.h"

namespace tiny_dlna {

#if DLNA_USE_STD_VECTOR

/**
 * @brief Wrapper around std::vector that provides backward compatibility
 * with the legacy Vector API. This implementation delegates to std::vector
 * for all standard operations and adds compatibility methods like erase(int)
 * and reset() that were present in the original Vector implementation.
 * 
 * By using std::vector as the base, we get:
 * - Battle-tested memory management and RAII
 * - Full C++ standard library compliance
 * - Optimal performance
 * - Complete iterator support
 * 
 * @ingroup collections
 * @tparam T The type of elements stored in the vector
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T>
class Vector : public std::vector<T> {
 public:
  /// Inherit all constructors from std::vector
  using std::vector<T>::vector;
  
  /// Pull in base class erase methods (iterator-based overloads)
  using std::vector<T>::erase;

  /**
   * @brief Erase element by integer index (backward compatibility)
   * 
   * Removes the element at the specified index. This method is provided
   * for backward compatibility with code that uses integer indices instead
   * of iterators.
   * 
   * @param index The zero-based index of the element to remove
   */
  void erase(int index) {
    if (index < 0 || index >= this->size()) return;
    std::vector<T>::erase(this->begin() + index);
  }
  
  /**
   * @brief Reset the vector (backward compatibility)
   * 
   * Clears all elements and releases allocated memory by shrinking the
   * capacity to zero. This method is provided for backward compatibility
   * with the legacy Vector implementation.
   */
  void reset() {
    this->clear();
    this->shrink_to_fit();
  }
};

#else

/**
 * @brief Custom Vector implementation that provides std::vector-like API
 * with support for custom allocators. This implementation is used when
 * DLNA_USE_STD_VECTOR is disabled, typically for embedded systems where
 * std::vector is not available or custom memory management is required.
 * 
 * This class provides the most important methods as defined by std::vector
 * and is more convenient than dealing with raw C arrays. It supports:
 * - Custom allocators for flexible memory management
 * - RAII with proper constructor/destructor calls
 * - Move semantics to avoid unnecessary copies
 * - Iterator support for range-based loops
 * 
 * @ingroup collections
 * @tparam T The type of elements stored in the vector
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

  /// Default constructor: size 0 with DefaultAllocator
  Vector(size_t len = 0, ALLOCATOR &allocator = DefaultAllocator) {
    setAllocator(allocator);
    resize_internal(len, false);
  }

  /// Constructor with only allocator
  Vector(ALLOCATOR &allocator) { setAllocator(allocator); }

  /// Allocate size and initialize array with default values
  Vector(int size, T value, ALLOCATOR &allocator = DefaultAllocator) {
    setAllocator(allocator);
    resize(size);
    for (int j = 0; j < size; j++) {
      p_data[j] = value;
    }
  }

  /// Move constructor - transfers ownership of data
  Vector(Vector<T> &&moveFrom) {
    swap(moveFrom);
    moveFrom.clear();
  };

  /// Move assignment operator - transfers ownership of data
  Vector &operator=(Vector &&moveFrom) {
    swap(moveFrom);
    moveFrom.clear();
    return *this;
  }

  /// Copy constructor - creates a deep copy
  Vector(Vector<T> &copyFrom) {
    this->p_allocator = copyFrom.p_allocator;
    resize_internal(copyFrom.size(), false);
    for (int j = 0; j < copyFrom.size(); j++) {
      p_data[j] = copyFrom[j];
    }
    this->len = copyFrom.size();
  }

  /// Copy assignment operator - creates a deep copy
  Vector<T> &operator=(Vector<T> &copyFrom) {
    resize_internal(copyFrom.size(), false);
    for (int j = 0; j < copyFrom.size(); j++) {
      p_data[j] = copyFrom[j];
    }
    this->len = copyFrom.size();
    return *this;
  }

  /// Legacy constructor with pointer range
  Vector(T *from, T *to, ALLOCATOR &allocator = DefaultAllocator) {
    this->p_allocator = &allocator;
    this->len = to - from;
    resize_internal(this->len, false);
    for (size_t j = 0; j < this->len; j++) {
      p_data[j] = from[j];
    }
  }

  /// Destructor - calls reset() to clean up all resources
  virtual ~Vector() { reset(); }

  /// Set the allocator (use with caution)
  void setAllocator(ALLOCATOR &allocator) { p_allocator = &allocator; }

  /// Clear all elements, calling destructors but keeping capacity
  void clear() {
    // Call destructors on all active elements before clearing
    if (p_data != nullptr) {
      cleanup(p_data, 0, len);
    }
    len = 0;
  }

  /// Get the number of elements
  int size() { return len; }

  /// Check if the vector is empty
  bool empty() { return size() == 0; }

  /// Add element to the end (move semantics)
  void push_back(T &&value) {
    resize_internal(len + 1, true);
    p_data[len] = value;
    len++;
  }

  /// Add element to the end (copy semantics)
  void push_back(T &value) {
    resize_internal(len + 1, true);
    p_data[len] = value;
    len++;
  }

  /// Add element to the front (copy semantics)
  void push_front(T &value) {
    resize_internal(len + 1, true);
    // memmove(p_data,p_data+1,len*sizeof(T));
    for (int j = len; j >= 0; j--) {
      p_data[j + 1] = p_data[j];
    }
    p_data[0] = value;
    len++;
  }

  /// Add element to the front (move semantics)
  void push_front(T &&value) {
    resize_internal(len + 1, true);
    // memmove(p_data,p_data+1,len*sizeof(T));
    for (int j = len; j >= 0; j--) {
      p_data[j + 1] = p_data[j];
    }
    p_data[0] = value;
    len++;
  }

  /// Remove the last element
  void pop_back() {
    if (len > 0) {
      len--;
    }
  }

  /// Remove the first element
  void pop_front() { erase(0); }

  /// Assign from iterator range
  void assign(Iterator v1, Iterator v2) {
    size_t newLen = v2 - v1;
    resize_internal(newLen, false);
    this->len = newLen;
    int pos = 0;
    for (auto ptr = v1; ptr != v2; ptr++) {
      p_data[pos++] = *ptr;
    }
  }

  /// Assign multiple copies of a value
  void assign(size_t number, T value) {
    resize_internal(number, false);
    this->len = number;
    for (int j = 0; j < number; j++) {
      p_data[j] = value;
    }
  }

  /// Swap contents with another vector
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

  /// Access element by index (non-const)
  T &operator[](int index) {
    assert(p_data != nullptr);
    return p_data[index];
  }

  /// Access element by index (const)
  T &operator[](const int index) const { return p_data[index]; }

  /// Resize and fill with value
  bool resize(int newSize, T value) {
    if (resize(newSize)) {
      for (int j = 0; j < newSize; j++) {
        p_data[j] = value;
      }
      return true;
    }
    return false;
  }

  /// Reduce capacity to match size
  void shrink_to_fit() { resize_internal(this->len, true, true); }

  /// Get the current capacity
  int capacity() { return this->max_capacity; }

  /// Resize the vector to newSize elements
  bool resize(int newSize) {
    int oldSize = this->len;
    resize_internal(newSize, true);
    this->len = newSize;
    return this->len != oldSize;
  }

  /// Get iterator to beginning
  Iterator begin() { return Iterator(p_data, 0); }

  /// Get reference to last element
  T &back() { return *Iterator(p_data + (len - 1), len - 1); }

  /// Get iterator to end
  Iterator end() { return Iterator(p_data + len, len); }

  /// Remove element at iterator position
  void erase(Iterator it) { return erase(it.pos()); }

  /// Remove element at index position
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

  /// Get pointer to underlying array
  T *data() { return p_data; }
  
  /// Get pointer to underlying array (const)
  T *data() const { return p_data; }

  /// Check if the vector is valid (has allocated data)
  operator bool() const { return p_data != nullptr; }

  /// Find the index of an element (-1 if not found)
  int indexOf(T obj) {
    for (int j = 0; j < size(); j++) {
      if (p_data[j] == obj) return j;
    }
    return -1;
  }

  /// Check if vector contains an element
  bool contains(T obj) { return indexOf(obj) >= 0; }

  /**
   * @brief Reset the vector - destroy all elements and deallocate memory
   * 
   * This method ensures all elements are destroyed by calling their destructors
   * and then frees the underlying buffer. After reset(), the vector will have
   * size 0 and capacity 0.
   */
  void reset() {
    // Ensure all elements are destroyed and the underlying buffer is freed.
    // Previously this called deleteArray(p_data, size()) which used the
    // current logical length (`len`) instead of the allocated buffer
    // capacity (`max_capacity`). That caused the allocated buffer to
    // remain unfreed when len was 0 (leading to a memory leak).
    if (p_data != nullptr) {
      // Call destructors on all active elements to prevent memory leaks
      cleanup(p_data, 0, len);
      // free the full allocated buffer
      deleteArray(p_data, max_capacity);
      p_data = nullptr;
      max_capacity = 0;
      len = 0;
    }
  }

 protected:
  int max_capacity = 0;  ///< Maximum number of elements that can be stored without reallocation
  int len = 0;           ///< Current number of elements in the vector
  T *p_data = nullptr;   ///< Pointer to the underlying data array
  ALLOCATOR *p_allocator = &DefaultAllocator;  ///< Pointer to the allocator instance

  /**
   * @brief Internal resize implementation with control over copying and shrinking
   * 
   * @param newSize The new capacity to allocate
   * @param copy If true, copy existing elements to the new buffer
   * @param shrink If true, allow shrinking the capacity
   */
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
          // Element-wise copy using assignment/constructors. Using memmove
          // here is unsafe for non-trivial types (it copies internal
          // pointers/ownership and can lead to double-free). Do a proper
          // element-wise copy so constructors/assignments run.
          for (int j = 0; j < len; j++) {
            p_data[j] = oldData[j];
          }
        }
        // Clean up old elements before deallocating, but only those we didn't copy
        if (!copy && len > 0) {
          // If we're not copying, destroy all old elements
          cleanup(oldData, 0, len);
        }
        if (shrink) {
          cleanup(oldData, newSize, oldBufferLen);
        }
        deleteArray(oldData, oldBufferLen);  // delete [] oldData;
      }
    }
  }

  /**
   * @brief Allocate a new array of elements using the allocator
   * 
   * @param newSize Number of elements to allocate
   * @return Pointer to the allocated array
   */
  T *newArray(int newSize) {
    T *data;
#if DLNA_USE_ALLOCATOR
    data = p_allocator->createArray<T>(newSize);  // new T[newSize+1];
#else
    data = new T[newSize];
#endif
    return data;
  }

  /**
   * @brief Deallocate an array using the allocator
   * 
   * @param oldData Pointer to the array to deallocate
   * @param oldBufferLen Number of elements in the array
   */
  void deleteArray(T *oldData, int oldBufferLen) {
    if (oldData == nullptr) return;
#if DLNA_USE_ALLOCATOR
    p_allocator->removeArray(oldData, oldBufferLen);  // delete [] oldData;
#else
    delete[] oldData;
#endif
  }

  /**
   * @brief Call destructors on a range of elements
   * 
   * @param data Pointer to the array
   * @param from Starting index (inclusive)
   * @param to Ending index (exclusive)
   */
  void cleanup(T *data, int from, int to) {
    for (int j = from; j < to; j++) {
      data[j].~T();
    }
  }
};

#endif

}  // namespace tiny_dlna