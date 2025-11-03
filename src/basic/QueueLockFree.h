
#pragma once
#include <stdint.h>

#include <atomic>
#include <cstddef>
#include <utility>

namespace tiny_dlna {

/**
 * @brief A simple single producer, single consumer lock free queue
 */
template <typename T>
class QueueLockFree {
 public:
  QueueLockFree(size_t capacity, ALLOCATOR& allocator = DefaultAllocator) {
    setAllocator(allocator);
    resize(capacity);
  }

  ~QueueLockFree() {
    // destroy any remaining constructed elements between head and tail
    size_t head = head_pos.load(std::memory_order_relaxed);
    size_t tail = tail_pos.load(std::memory_order_relaxed);
    for (size_t i = head; i != tail; ++i)
      (&p_node[i & capacity_mask].data)->~T();
  }

  void setAllocator(ALLOCATOR& allocator) { vector.setAllocator(allocator); }

  void resize(size_t capacity) {
    // round up to next power-of-two and compute mask/value
    capacity_mask = capacity - 1;
    for (size_t i = 1; i <= sizeof(void*) * 4; i <<= 1)
      capacity_mask |= capacity_mask >> i;
    capacity_value = capacity_mask + 1;

    // allocate vector with the power-of-two capacity_value
    vector.resize(capacity_value);
    p_node = vector.data();

    // initialize nodes for the full allocated capacity
    for (size_t i = 0; i < capacity_value; ++i) {
      p_node[i].tail.store(i, std::memory_order_relaxed);
      p_node[i].head.store((size_t)-1, std::memory_order_relaxed);
    }

    tail_pos.store(0, std::memory_order_relaxed);
    head_pos.store(0, std::memory_order_relaxed);
  }

  size_t capacity() const { return capacity_value; }

  size_t size() const {
    size_t head = head_pos.load(std::memory_order_acquire);
    return tail_pos.load(std::memory_order_relaxed) - head;
  }

  bool enqueue( T&& data) {
    Node* node;
    size_t tail = tail_pos.load(std::memory_order_relaxed);
    for (;;) {
      node = &p_node[tail & capacity_mask];
      if (node->tail.load(std::memory_order_relaxed) != tail) return false;
      if ((tail_pos.compare_exchange_weak(tail, tail + 1,
                                          std::memory_order_relaxed)))
        break;
    }
    // move-construct into the node to avoid an extra copy (important in
    // async/interrupt contexts where the source may have already allocated)
    new (&node->data) T(std::move(data));
    node->head.store(tail, std::memory_order_release);
    return true;
  }

  bool enqueue( T& data) {
    Node* node;
    size_t tail = tail_pos.load(std::memory_order_relaxed);
    for (;;) {
      node = &p_node[tail & capacity_mask];
      if (node->tail.load(std::memory_order_relaxed) != tail) return false;
      if ((tail_pos.compare_exchange_weak(tail, tail + 1,
                                          std::memory_order_relaxed)))
        break;
    }
    new (&node->data) T(data);
    node->head.store(tail, std::memory_order_release);
    return true;
  }

  bool dequeue(const T&& data) {
    return dequeue(data);
  }

  bool dequeue(T& result) {
    Node* node;
    size_t head = head_pos.load(std::memory_order_relaxed);
    for (;;) {
      node = &p_node[head & capacity_mask];
      if (node->head.load(std::memory_order_relaxed) != head) return false;
      if (head_pos.compare_exchange_weak(head, head + 1,
                                         std::memory_order_relaxed))
        break;
    }
    // move-assign to avoid an extra copy when possible
    result = std::move(node->data);
    (&node->data)->~T();
    node->tail.store(head + capacity_value, std::memory_order_release);
    return true;
  }

  void clear() {
    T tmp;
    while (dequeue(tmp));
  }

 protected:
  /// QueueLockFree Node
  struct Node {
    T data;
    std::atomic<size_t> tail;
    std::atomic<size_t> head;
  };

  Node* p_node;
  size_t capacity_mask;
  size_t capacity_value;
  std::atomic<size_t> tail_pos;
  std::atomic<size_t> head_pos;
  Vector<Node> vector;
};
}  // namespace audio_tools