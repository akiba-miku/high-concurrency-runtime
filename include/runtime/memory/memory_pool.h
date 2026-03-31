#pragma once

#include "runtime/base/noncopyable.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <utility>

namespace runtime::memory {

// Thread-safe fixed-size memory pool for efficient object allocation.
// Template parameters:
//   T - Type of objects to allocate
//   Capacity - Maximum number of objects that can be allocated (default: 1024)
//
// Example usage:
//   MemoryPool<MyClass, 128> pool;
//   MyClass* obj = pool.Construct(arg1, arg2);
//   pool.Destroy(obj);
//
// Thread safety:
//   All public methods are thread-safe and can be called concurrently.
template <typename T, std::size_t Capacity = 1024>
class MemoryPool : public runtime::base::NonCopyable {
 public:
  MemoryPool() { Initialize(); }

  ~MemoryPool() {
    ::operator delete(buffer_, std::align_val_t{kAlignment});
  }

  // Allocates a memory slot from the pool.
  // Returns nullptr if pool is exhausted.
  void* Allocate() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (free_list_head_ == nullptr) {
      return nullptr;
    }

    void* ptr = free_list_head_;
    free_list_head_ = *static_cast<void**>(free_list_head_);
    --free_count_;
    return ptr;
  }

  // Returns a memory slot back to the pool.
  // Passing nullptr is a no-op.
  void Deallocate(void* ptr) noexcept {
    if (ptr == nullptr) {
      return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    *static_cast<void**>(ptr) = free_list_head_;
    free_list_head_ = ptr;
    ++free_count_;
  }

  // Constructs an object of type T in the pool using forwarded arguments.
  // Returns nullptr if pool is exhausted.
  // Throws: Any exception thrown by T's constructor.
  template <typename... Args>
  T* Construct(Args&&... args) {
    void* mem = Allocate();
    if (mem == nullptr) {
      return nullptr;
    }

    try {
      return new (mem) T(std::forward<Args>(args)...);
    } catch (...) {
      Deallocate(mem);
      throw;
    }
  }

  // Destroys an object and returns its memory to the pool.
  // Passing nullptr is a no-op.
  void Destroy(T* ptr) noexcept {
    if (ptr == nullptr) {
      return;
    }

    ptr->~T();
    Deallocate(ptr);
  }

  // Returns the maximum capacity of the pool.
  constexpr std::size_t capacity() const noexcept { return Capacity; }

  // Returns the current number of free slots in the pool.
  std::size_t free_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return free_count_;
  }

  // Returns the current number of used slots in the pool.
  std::size_t used_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return Capacity - free_count_;
  }

  // Checks if a pointer was allocated from this pool.
  bool owns(const void* ptr) const noexcept {
    if (ptr == nullptr || buffer_ == nullptr) {
      return false;
    }

    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(buffer_);
    const std::uintptr_t end = begin + kSlotSize * Capacity;
    const std::uintptr_t p = reinterpret_cast<std::uintptr_t>(ptr);

    if (p < begin || p >= end) {
      return false;
    }

    return (p - begin) % kSlotSize == 0;
  }

 private:
  static constexpr std::size_t kAlignment =
      std::max(alignof(T), alignof(void*));

  static constexpr std::size_t kSlotSize =
      ((std::max(sizeof(T), sizeof(void*)) + kAlignment - 1) / kAlignment) *
      kAlignment;

  // Initializes the memory pool and builds the free list.
  void Initialize() {
    buffer_ = static_cast<std::byte*>(
        ::operator new(kSlotSize * Capacity, std::align_val_t{kAlignment}));

    std::byte* current = buffer_;
    for (std::size_t i = 0; i < Capacity - 1; ++i) {
      std::byte* next = current + kSlotSize;
      *reinterpret_cast<void**>(current) = next;
      current = next;
    }

    *reinterpret_cast<void**>(current) = nullptr;
    free_list_head_ = buffer_;
    free_count_ = Capacity;
  }

  // Raw memory buffer for the pool.
  std::byte* buffer_{nullptr};

  // Head of the singly-linked free list.
  void* free_list_head_{nullptr};

  // Current number of free slots in the pool.
  std::size_t free_count_{0};

  // Mutex protecting all internal state.
  // mutable to allow locking in const methods.
  mutable std::mutex mutex_;
};

}  // namespace runtime::memory
