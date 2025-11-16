#pragma once
#include <stdlib.h>
#include <memory>
#include <limits>
#include <assert.h>
#include <type_traits>

#ifdef ESP32
#include <esp_heap_caps.h>
#endif
#include "dlna_config.h"

namespace tiny_dlna {

/**
 * @class AllocatorPSRAM
 * @brief Custom allocator that uses ESP32's PSRAM for memory allocation
 * @tparam T Type of elements to allocate
 *
 * This allocator uses ESP32's heap_caps_malloc with MALLOC_CAP_SPIRAM flag
 * to ensure all memory is allocated in PSRAM instead of regular RAM.
 */
template <typename T>
class AllocatorPSRAM {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using is_always_equal = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;

  /**
   * @brief Default constructor
   */
  AllocatorPSRAM() noexcept {}

  /**
   * @brief Copy constructor from another allocator type
   * @tparam U Type of the other allocator
   * @param other The other allocator
   */
  template <typename U>
  AllocatorPSRAM(const AllocatorPSRAM<U>&) noexcept {}

  /**
   * @brief Allocate memory from PSRAM
   * @param n Number of elements to allocate
   * @return Pointer to allocated memory
   * @throws std::bad_alloc If allocation fails or size is too large
   */
  pointer allocate(size_type n) {
    if (n==0) return nullptr;
    // if (n > std::numeric_limits<size_type>::max() / sizeof(T))
    //     throw std::bad_alloc();
    // in Arduino excepitons are disabled!
    assert(n <= std::numeric_limits<size_type>::max() / sizeof(T));

#ifdef ESP32
    pointer p = static_cast<pointer>(
        heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM));
    if (p == nullptr) {
      p = static_cast<pointer>(malloc(n * sizeof(T)));
    }
    assert(p);
#else
    pointer p = static_cast<pointer>(malloc(n * sizeof(T)));
    if (!p) throw std::bad_alloc();
#endif
    return p;
  }

  /**
   * @brief Deallocate memory
   * @param p Pointer to memory to deallocate
   * @param size Size of allocation (unused)
   */
  void deallocate(pointer p, size_type) noexcept {
#ifdef ESP32
    if (p) heap_caps_free(p);
#else
    if (p) free(p);
#endif
  }

  /**
   * @brief Rebind allocator to another type
   * @tparam U Type to rebind the allocator to
   */
  template <typename U>
  struct rebind {
    using other = AllocatorPSRAM<U>;
  };

  // Allocators compare equal (stateless)
  template <typename U>
  bool operator==(const AllocatorPSRAM<U>&) const noexcept { return true; }
  template <typename U>
  bool operator!=(const AllocatorPSRAM<U>&) const noexcept { return false; }
};

// Alias for a std::string that stores its dynamic buffer in PSRAM
using stringPSRAM = std::basic_string<char, std::char_traits<char>, AllocatorPSRAM<char>>;



}  // namespace tiny_dlna