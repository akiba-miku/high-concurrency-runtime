#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/memory/memory_pool.h"

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

namespace runtime::memory {

// ObjectPool is a thin RAII wrapper over MemoryPool.
// It keeps the same fixed-capacity behavior, while providing:
//   1. Acquire/Release object-oriented naming
//   2. ScopedPtr for automatic return-to-pool on scope exit
//
// Example:
//   ObjectPool<MyType, 128> pool;
//   auto ptr = pool.AcquireScoped(arg1, arg2);
//   if (ptr) {
//     ptr->DoSomething();
//   }  // automatically returned to the pool here
template <typename T, std::size_t Capacity = 1024>
class ObjectPool : public runtime::base::NonCopyable {
 public:
  class Deleter {
   public:
    Deleter() = default;
    explicit Deleter(ObjectPool* pool) noexcept : pool_(pool) {}

    void operator()(T* ptr) const noexcept {
      if (pool_ != nullptr && ptr != nullptr) {
        pool_->Release(ptr);
      }
    }

   private:
    ObjectPool* pool_{nullptr};
  };

  using ScopedPtr = std::unique_ptr<T, Deleter>;

  ObjectPool() = default;
  ~ObjectPool() = default;

  // Constructs an object from the pool.
  // Returns nullptr if the pool is exhausted.
  // Throws: Any exception thrown by T's constructor.
  template <typename... Args>
  T* Acquire(Args&&... args) {
    return memory_pool_.Construct(std::forward<Args>(args)...);
  }

  // Constructs an object and wraps it in a unique_ptr-like handle
  // that automatically returns the object to the pool.
  template <typename... Args>
  ScopedPtr AcquireScoped(Args&&... args) {
    return ScopedPtr(Acquire(std::forward<Args>(args)...), Deleter(this));
  }

  // Destroys the object and returns its slot back to the pool.
  void Release(T* ptr) noexcept {
    if (ptr == nullptr) {
      return;
    }

    assert(memory_pool_.owns(ptr) && "pointer does not belong to this ObjectPool");
    memory_pool_.Destroy(ptr);
  }

  // Returns true if the pointer belongs to this pool.
  bool owns(const T* ptr) const noexcept {
    return memory_pool_.owns(ptr);
  }

  // Expose the same capacity stats as MemoryPool.
  constexpr std::size_t capacity() const noexcept {
    return memory_pool_.capacity();
  }

  std::size_t free_count() const noexcept {
    return memory_pool_.free_count();
  }

  std::size_t used_count() const noexcept {
    return memory_pool_.used_count();
  }

 private:
  MemoryPool<T, Capacity> memory_pool_;
};

}  // namespace runtime::memory
