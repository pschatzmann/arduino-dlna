#include "dlna_config.h"
#include "Allocator.h"

#pragma once
#include <assert.h>
#include <vector>
#include <memory>

namespace tiny_dlna {


template <class T, class Alloc = ALLOCATOR<T>>
class Vector : public std::vector<T, Alloc> {
 public:
  using Base = std::vector<T, Alloc>;
  using Base::Base;
  using value_type = T;
  using iterator = typename Base::iterator;
  using const_iterator = typename Base::const_iterator;
  // expose base erase overloads (iterator, range) alongside our index helper
  using Base::erase;

  Vector() = default;
  explicit Vector(size_t count) : Base(count) {}
  Vector(size_t count, const T &value) : Base(count, value) {}
  template <class It>
  Vector(It first, It last) : Base(first, last) {}
  Vector(std::initializer_list<T> il) : Base(il) {}
  explicit Vector(const Alloc &alloc) : Base(alloc) {}

  // Integer index erase helper (optional convenience)
  void eraseIndex(int index) {
    if (index < 0 || static_cast<size_t>(index) >= this->size()) return;
    Base::erase(this->begin() + index);
  }

  // Backwards-compatible erase by index returning iterator
  iterator erase(size_t index) {
    if (index >= this->size()) return this->end();
    return Base::erase(this->begin() + static_cast<typename Base::difference_type>(index));
  }

  // Reset helper: clear + shrink
  void reset() {
    this->clear();
    this->shrink_to_fit();
  }
};
// Legacy custom Vector implementation removed.

}  // namespace tiny_dlna