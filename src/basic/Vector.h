#include "dlna_config.h"
#include "Allocator.h"

#pragma once
#include <assert.h>
#include <vector>
#include <memory>

namespace tiny_dlna {

/** 
 * @class Vector
 * @brief Lightweight wrapper around std::vector with Arduino-friendly helpers and a pluggable allocator.
 *
 * This container inherits from std::vector to preserve its API and complexity guarantees while
 * adding a few conveniences tailored for embedded/Arduino targets:
 *
 * - Uses a configurable allocator (default: ALLOCATOR<T>) defined by the library to enable
 *   custom allocation strategies or tracking.
 * - Keeps all standard std::vector constructors, types, and methods available.
 * - Exposes std::vector erase overloads and provides index-based helpers.
 * - Adds reset() which performs clear() followed by shrink_to_fit() to aggressively release memory.
 *
 * Template parameters:
 * - @tparam T     Element/value type stored in the vector.
 * - @tparam Alloc Allocator type (defaults to ALLOCATOR<T> as configured in dlna_config.h).
 *
 * Differences vs std::vector:
 * - erase(size_t index): convenience overload that erases an element by index and returns the
 *   iterator following the erased element (or end() if the last element was erased). If the index
 *   is out of range, end() is returned and no action is taken.
 * - eraseIndex(int index): bounds-checked helper; does nothing on out-of-range indices.
 *
 * Complexity: Matches std::vector for the corresponding operations (e.g. erase is linear in the
 * number of moved elements). Reserve/capacity/iterator invalidation rules are also identical to
 * std::vector.
 */
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

  /**
   * @brief Erase element by index (no-op on out-of-range).
   * @param index Zero-based index of the element to erase.
   * @note Equivalent to erase(begin() + index) when in range; does nothing otherwise.
   */
  void eraseIndex(int index) {
    if (index < 0 || static_cast<size_t>(index) >= this->size()) return;
    Base::erase(this->begin() + index);
  }

  /**
   * @brief Convenience overload to erase by index.
   * @param index Zero-based index to erase.
   * @return iterator Iterator to the element following the erased one, or end() if none.
   * @note Returns end() and performs no action if the index is out of range.
   */
  iterator erase(size_t index) {
    if (index >= this->size()) return this->end();
    return Base::erase(this->begin() + static_cast<typename Base::difference_type>(index));
  }

  /**
   * @brief Reset the container by clearing and shrinking capacity to fit.
   * @details Calls clear() followed by shrink_to_fit() to return memory to the system where
   *          supported. Useful on memory-constrained targets after large, temporary usage.
   */
  void reset() {
    this->clear();
    this->shrink_to_fit();
  }
};

}  // namespace tiny_dlna