#pragma once
#include "List.h"

namespace tiny_dlna {

/**
 * @brief FIFO Queue which is based on a List
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T>
class Queue {
 public:
  Queue() = default;

  // enqueue by non-const lvalue reference (matches existing copy ctor semantics)
  bool enqueue(T& data) { return l.push_front(data); }

  // enqueue by move - avoids copying/memory allocation when possible
  bool enqueue(T&& data) { return l.push_front(std::move(data)); }

  bool peek(T& data) {
    if (l.end()->prior == nullptr) return false;
    data = *(l.end()->prior);
    return true;
  }

  bool dequeue(T& data) { return l.pop_back(data); }

  size_t size() { return l.size(); }

  bool clear() { return l.clear(); }

  bool empty() { return l.empty(); }

  void setAllocator(ALLOCATOR& allocator) { l.setAllocator(allocator); }

 protected:
  List<T> l;
};

}  // namespace tiny_dlna