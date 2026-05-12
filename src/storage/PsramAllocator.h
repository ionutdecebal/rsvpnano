#pragma once

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

template <typename T>
class PsramAllocator {
 public:
  using value_type = T;

  PsramAllocator() noexcept = default;

  template <typename U>
  PsramAllocator(const PsramAllocator<U> &) noexcept {}

  template <typename U>
  struct rebind {
    using other = PsramAllocator<U>;
  };

  T *allocate(std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      throw std::bad_alloc();
    }

    const std::size_t bytes = count * sizeof(T);
#if defined(ARDUINO_ARCH_ESP32)
    void *ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == nullptr) {
      ptr = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }
#else
    void *ptr = std::malloc(bytes);
#endif
    if (ptr == nullptr) {
      throw std::bad_alloc();
    }
    return static_cast<T *>(ptr);
  }

  void deallocate(T *ptr, std::size_t) noexcept {
#if defined(ARDUINO_ARCH_ESP32)
    heap_caps_free(ptr);
#else
    std::free(ptr);
#endif
  }

  template <typename U>
  bool operator==(const PsramAllocator<U> &) const noexcept {
    return true;
  }

  template <typename U>
  bool operator!=(const PsramAllocator<U> &) const noexcept {
    return false;
  }
};
