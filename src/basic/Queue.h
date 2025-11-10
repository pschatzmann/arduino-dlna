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
template <class T, class Alloc = DLNA_ALLOCATOR<T>>
class Queue {
 public:
  Queue() = default;
  explicit Queue(const Alloc& alloc) : l_instance(alloc) {}

  // enqueue by non-const lvalue reference (matches existing copy ctor semantics)
  bool enqueue(T& data) { return l_instance.push_front(data); }

  // enqueue by move - avoids copying/memory allocation when possible
  bool enqueue(T&& data) { return l_instance.push_front(std::move(data)); }

  bool peek(T& data) {
    if (l_instance.end()->prior == nullptr) return false;
    data = *(l_instance.end()->prior);
    return true;
  }

  bool dequeue(T& data) { return l_instance.pop_back(data); }

  size_t size() { return l_instance.size(); }

  bool clear() { return l_instance.clear(); }

  bool empty() { return l_instance.empty(); }

  // setAllocator removed (legacy allocator support dropped)

 protected:
  List<T, Alloc> l_instance;  // underlying allocator-aware list
};

}  // namespace tiny_dlna