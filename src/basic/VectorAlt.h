#pragma once

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "basic/Logger.h"
#include "Allocator.h"

namespace tiny_dlna {

/**
 * @brief Vector implementation that provides std::vector-like API with proper
 * RAII semantics and memory management. This implementation handles resource
 * cleanup correctly through destructors and supports both trivial and
 * non-trivial types. Supports custom allocators for flexible memory management.
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/
template <class T, class Allocator = ALLOCATOR>
class Vector {
 public:
  // Type aliases (matching std::vector)
  using value_type = T;
  using allocator_type = Allocator;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;

  /**
   * @brief Iterator for Vector<T> with full random access support
   */
  class Iterator {
   protected:
    T* ptr_;
    size_t pos_;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    Iterator() : ptr_(nullptr), pos_(0) {}
    Iterator(T* ptr, size_t pos) : ptr_(ptr), pos_(pos) {}

    // Copy constructor
    Iterator(const Iterator& other) : ptr_(other.ptr_), pos_(other.pos_) {}

    // Assignment
    Iterator& operator=(const Iterator& other) {
      ptr_ = other.ptr_;
      pos_ = other.pos_;
      return *this;
    }

    // Dereference
    reference operator*() const { return *ptr_; }
    pointer operator->() const { return ptr_; }
    reference operator[](difference_type n) const { return ptr_[n]; }

    // Increment/Decrement
    Iterator& operator++() {
      ++ptr_;
      ++pos_;
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    Iterator& operator--() {
      --ptr_;
      --pos_;
      return *this;
    }
    Iterator operator--(int) {
      Iterator tmp = *this;
      --(*this);
      return tmp;
    }

    // Arithmetic
    Iterator& operator+=(difference_type n) {
      ptr_ += n;
      pos_ += n;
      return *this;
    }
    Iterator& operator-=(difference_type n) {
      ptr_ -= n;
      pos_ -= n;
      return *this;
    }
    Iterator operator+(difference_type n) const {
      return Iterator(ptr_ + n, pos_ + n);
    }
    Iterator operator-(difference_type n) const {
      return Iterator(ptr_ - n, pos_ - n);
    }
    difference_type operator-(const Iterator& other) const {
      return ptr_ - other.ptr_;
    }

    // Comparison
    bool operator==(const Iterator& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const Iterator& other) const { return ptr_ != other.ptr_; }
    bool operator<(const Iterator& other) const { return ptr_ < other.ptr_; }
    bool operator<=(const Iterator& other) const { return ptr_ <= other.ptr_; }
    bool operator>(const Iterator& other) const { return ptr_ > other.ptr_; }
    bool operator>=(const Iterator& other) const { return ptr_ >= other.ptr_; }

    // Accessors
    pointer ptr() const { return ptr_; }
    size_t pos() const { return pos_; }
  };

  /**
   * @brief Const iterator for Vector<T>
   */
  class ConstIterator {
   protected:
    const T* ptr_;
    size_t pos_;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

    ConstIterator() : ptr_(nullptr), pos_(0) {}
    ConstIterator(const T* ptr, size_t pos) : ptr_(ptr), pos_(pos) {}
    ConstIterator(const Iterator& it) : ptr_(it.ptr()), pos_(it.pos()) {}

    reference operator*() const { return *ptr_; }
    pointer operator->() const { return ptr_; }
    reference operator[](difference_type n) const { return ptr_[n]; }

    ConstIterator& operator++() {
      ++ptr_;
      ++pos_;
      return *this;
    }
    ConstIterator operator++(int) {
      ConstIterator tmp = *this;
      ++(*this);
      return tmp;
    }
    ConstIterator& operator--() {
      --ptr_;
      --pos_;
      return *this;
    }
    ConstIterator operator--(int) {
      ConstIterator tmp = *this;
      --(*this);
      return tmp;
    }

    ConstIterator& operator+=(difference_type n) {
      ptr_ += n;
      pos_ += n;
      return *this;
    }
    ConstIterator& operator-=(difference_type n) {
      ptr_ -= n;
      pos_ -= n;
      return *this;
    }
    ConstIterator operator+(difference_type n) const {
      return ConstIterator(ptr_ + n, pos_ + n);
    }
    ConstIterator operator-(difference_type n) const {
      return ConstIterator(ptr_ - n, pos_ - n);
    }
    difference_type operator-(const ConstIterator& other) const {
      return ptr_ - other.ptr_;
    }

    bool operator==(const ConstIterator& other) const {
      return ptr_ == other.ptr_;
    }
    bool operator!=(const ConstIterator& other) const {
      return ptr_ != other.ptr_;
    }
    bool operator<(const ConstIterator& other) const {
      return ptr_ < other.ptr_;
    }
    bool operator<=(const ConstIterator& other) const {
      return ptr_ <= other.ptr_;
    }
    bool operator>(const ConstIterator& other) const {
      return ptr_ > other.ptr_;
    }
    bool operator>=(const ConstIterator& other) const {
      return ptr_ >= other.ptr_;
    }

    pointer ptr() const { return ptr_; }
    size_t pos() const { return pos_; }
  };

  // ==========================================================================
  // Constructors and Destructor
  // ==========================================================================

  /// Default constructor
  Vector() 
      : data_(nullptr), size_(0), capacity_(0), allocator_(&DefaultAllocator) {}

  /// Constructor with allocator
  explicit Vector(Allocator& alloc)
      : data_(nullptr), size_(0), capacity_(0), allocator_(&alloc) {}

  /// Constructor with size
  explicit Vector(size_type count, Allocator& alloc = DefaultAllocator)
      : data_(nullptr), size_(0), capacity_(0), allocator_(&alloc) {
    resize(count);
  }

  /// Constructor with size and default value
  Vector(size_type count, const T& value, Allocator& alloc = DefaultAllocator)
      : data_(nullptr), size_(0), capacity_(0), allocator_(&alloc) {
    assign(count, value);
  }

  /// Constructor from iterator range
  template <typename InputIt>
  Vector(InputIt first, InputIt last, Allocator& alloc = DefaultAllocator)
      : data_(nullptr), size_(0), capacity_(0), allocator_(&alloc) {
    assign(first, last);
  }

  /// Copy constructor
  Vector(const Vector& other)
      : data_(nullptr), size_(0), capacity_(0), allocator_(other.allocator_) {
    reserve(other.size_);
    for (size_type i = 0; i < other.size_; ++i) {
      push_back(other.data_[i]);
    }
  }

  /// Move constructor
  Vector(Vector&& other) noexcept
      : data_(other.data_),
        size_(other.size_),
        capacity_(other.capacity_),
        allocator_(other.allocator_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  /// Destructor
  ~Vector() { destroy_and_deallocate(); }

  // ==========================================================================
  // Assignment operators
  // ==========================================================================

  /// Copy assignment
  Vector& operator=(const Vector& other) {
    if (this != &other) {
      clear();
      reserve(other.size_);
      for (size_type i = 0; i < other.size_; ++i) {
        push_back(other.data_[i]);
      }
    }
    return *this;
  }

  /// Move assignment
  Vector& operator=(Vector&& other) noexcept {
    if (this != &other) {
      destroy_and_deallocate();
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      other.data_ = nullptr;
      other.size_ = 0;
      other.capacity_ = 0;
    }
    return *this;
  }

  // ==========================================================================
  // Element access
  // ==========================================================================

  reference at(size_type pos) {
    assert(pos < size_);
    return data_[pos];
  }

  const_reference at(size_type pos) const {
    assert(pos < size_);
    return data_[pos];
  }

  reference operator[](size_type pos) {
    assert(pos < size_);
    return data_[pos];
  }

  const_reference operator[](size_type pos) const {
    assert(pos < size_);
    return data_[pos];
  }

  reference front() {
    assert(size_ > 0);
    return data_[0];
  }

  const_reference front() const {
    assert(size_ > 0);
    return data_[0];
  }

  reference back() {
    assert(size_ > 0);
    return data_[size_ - 1];
  }

  const_reference back() const {
    assert(size_ > 0);
    return data_[size_ - 1];
  }

  pointer data() noexcept { return data_; }
  const_pointer data() const noexcept { return data_; }

  // ==========================================================================
  // Iterators
  // ==========================================================================

  Iterator begin() noexcept { return Iterator(data_, 0); }
  ConstIterator begin() const noexcept { return ConstIterator(data_, 0); }
  ConstIterator cbegin() const noexcept { return ConstIterator(data_, 0); }

  Iterator end() noexcept { return Iterator(data_ + size_, size_); }
  ConstIterator end() const noexcept {
    return ConstIterator(data_ + size_, size_);
  }
  ConstIterator cend() const noexcept {
    return ConstIterator(data_ + size_, size_);
  }

  // ==========================================================================
  // Capacity
  // ==========================================================================

  bool empty() const noexcept { return size_ == 0; }
  size_type size() const noexcept { return size_; }
  size_type capacity() const noexcept { return capacity_; }

  void reserve(size_type new_cap) {
    if (new_cap > capacity_) {
      reallocate(new_cap);
    }
  }

  void shrink_to_fit() {
    if (size_ < capacity_) {
      reallocate(size_);
    }
  }

  // ==========================================================================
  // Modifiers
  // ==========================================================================

  void clear() noexcept {
    // Destroy all elements
    for (size_type i = 0; i < size_; ++i) {
      data_[i].~T();
    }
    size_ = 0;
  }

  Iterator insert(ConstIterator pos, const T& value) {
    size_type index = pos.pos();
    assert(index <= size_);

    if (size_ == capacity_) {
      size_type new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
      reserve(new_cap);
    }

    // Shift elements
    for (size_type i = size_; i > index; --i) {
      new (&data_[i]) T(std::move(data_[i - 1]));
      data_[i - 1].~T();
    }

    // Insert new element - use const_cast for compatibility
    new (&data_[index]) T(const_cast<T&>(value));
    ++size_;

    return Iterator(data_ + index, index);
  }

  Iterator insert(ConstIterator pos, T&& value) {
    size_type index = pos.pos();
    assert(index <= size_);

    if (size_ == capacity_) {
      size_type new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
      reserve(new_cap);
    }

    // Shift elements
    for (size_type i = size_; i > index; --i) {
      new (&data_[i]) T(std::move(data_[i - 1]));
      data_[i - 1].~T();
    }

    // Insert new element
    new (&data_[index]) T(std::move(value));
    ++size_;

    return Iterator(data_ + index, index);
  }

  Iterator erase(ConstIterator pos) {
    size_type index = pos.pos();
    assert(index < size_);

    // Destroy element
    data_[index].~T();

    // Shift remaining elements
    for (size_type i = index; i < size_ - 1; ++i) {
      new (&data_[i]) T(std::move(data_[i + 1]));
      data_[i + 1].~T();
    }

    --size_;
    return Iterator(data_ + index, index);
  }

  // Erase by index (for backward compatibility)
  Iterator erase(size_type index) {
    assert(index < size_);
    return erase(ConstIterator(data_ + index, index));
  }

  Iterator erase(ConstIterator first, ConstIterator last) {
    size_type first_idx = first.pos();
    size_type last_idx = last.pos();
    assert(first_idx <= last_idx && last_idx <= size_);

    size_type count = last_idx - first_idx;
    if (count == 0) return Iterator(data_ + first_idx, first_idx);

    // Destroy elements in range
    for (size_type i = first_idx; i < last_idx; ++i) {
      data_[i].~T();
    }

    // Shift remaining elements
    for (size_type i = first_idx; i < size_ - count; ++i) {
      new (&data_[i]) T(std::move(data_[i + count]));
      data_[i + count].~T();
    }

    size_ -= count;
    return Iterator(data_ + first_idx, first_idx);
  }

  void push_back(const T& value) {
    if (size_ == capacity_) {
      size_type new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
      reserve(new_cap);
    }
    // Use const_cast for compatibility with non-const copy constructors
    new (&data_[size_]) T(const_cast<T&>(value));
    ++size_;
  }

  void push_back(T&& value) {
    if (size_ == capacity_) {
      size_type new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
      reserve(new_cap);
    }
    new (&data_[size_]) T(std::move(value));
    ++size_;
  }

  template <typename... Args>
  reference emplace_back(Args&&... args) {
    if (size_ == capacity_) {
      size_type new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
      reserve(new_cap);
    }
    new (&data_[size_]) T(std::forward<Args>(args)...);
    ++size_;
    return data_[size_ - 1];
  }

  void pop_back() {
    assert(size_ > 0);
    --size_;
    data_[size_].~T();
  }

  void resize(size_type count) {
    if (count < size_) {
      // Destroy excess elements
      for (size_type i = count; i < size_; ++i) {
        data_[i].~T();
      }
      size_ = count;
    } else if (count > size_) {
      reserve(count);
      // Default construct new elements
      for (size_type i = size_; i < count; ++i) {
        new (&data_[i]) T();
      }
      size_ = count;
    }
  }

  void resize(size_type count, const T& value) {
    if (count < size_) {
      // Destroy excess elements
      for (size_type i = count; i < size_; ++i) {
        data_[i].~T();
      }
      size_ = count;
    } else if (count > size_) {
      reserve(count);
      // Copy construct new elements - use const_cast for compatibility
      for (size_type i = size_; i < count; ++i) {
        new (&data_[i]) T(const_cast<T&>(value));
      }
      size_ = count;
    }
  }

  void swap(Vector& other) noexcept {
    T* tmp_data = data_;
    size_type tmp_size = size_;
    size_type tmp_capacity = capacity_;

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = tmp_data;
    other.size_ = tmp_size;
    other.capacity_ = tmp_capacity;
  }

  void assign(size_type count, const T& value) {
    clear();
    reserve(count);
    for (size_type i = 0; i < count; ++i) {
      // Use const_cast for compatibility with non-const copy constructors
      new (&data_[i]) T(const_cast<T&>(value));
    }
    size_ = count;
  }

  template <typename InputIt>
  void assign(InputIt first, InputIt last) {
    clear();
    for (auto it = first; it != last; ++it) {
      push_back(*it);
    }
  }

  // ==========================================================================
  // Additional helper methods
  // ==========================================================================

  /// Get the allocator
  allocator_type& get_allocator() const { return *allocator_; }

  /// Set the allocator (use with caution, should only be called on empty vector)
  void set_allocator(Allocator& alloc) {
    assert(size_ == 0 && capacity_ == 0);
    allocator_ = &alloc;
  }

  /// Check if vector contains a value
  bool contains(const T& value) const {
    for (size_type i = 0; i < size_; ++i) {
      if (data_[i] == value) return true;
    }
    return false;
  }

  /// Find index of value, returns -1 if not found
  int indexOf(const T& value) const {
    for (size_type i = 0; i < size_; ++i) {
      if (data_[i] == value) return static_cast<int>(i);
    }
    return -1;
  }

  /// Boolean conversion
  explicit operator bool() const { return data_ != nullptr; }

  /// Reset the vector (clear and deallocate all memory)
  void reset() {
    destroy_and_deallocate();
  }

 protected:
  T* data_;
  size_type size_;
  size_type capacity_;
  Allocator* allocator_;

  /// Reallocate storage with proper element handling
  void reallocate(size_type new_capacity) {
    T* new_data = allocate(new_capacity);

    // Move existing elements to new storage
    for (size_type i = 0; i < size_; ++i) {
      new (&new_data[i]) T(std::move(data_[i]));
      data_[i].~T();
    }

    // Deallocate old storage
    deallocate(data_, capacity_);

    data_ = new_data;
    capacity_ = new_capacity;
  }

  /// Allocate raw memory (does not construct elements)
  T* allocate(size_type n) {
    if (n == 0) return nullptr;
#if DLNA_USE_ALLOCATOR
    return allocator_->template createArray<T>(n);
#else
    return static_cast<T*>(::operator new(n * sizeof(T)));
#endif
  }

  /// Deallocate raw memory (does not destroy elements)
  void deallocate(T* ptr, size_type n) {
    if (ptr) {
#if DLNA_USE_ALLOCATOR
      allocator_->removeArray(ptr, n);
#else
      ::operator delete(ptr);
#endif
    }
  }

  /// Destroy all elements and deallocate storage
  void destroy_and_deallocate() {
    if (data_) {
      // Destroy all elements
      for (size_type i = 0; i < size_; ++i) {
        data_[i].~T();
      }
      // Deallocate storage
      deallocate(data_, capacity_);
      data_ = nullptr;
      size_ = 0;
      capacity_ = 0;
    }
  }
};

}  // namespace tiny_dlna