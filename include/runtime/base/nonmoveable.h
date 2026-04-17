#pragma once

namespace runtime::base {

// NonCopyable disables move construction and move assignment for derived
// types.
class NonMoveable {
protected:
  explicit NonMoveable() = default;
  ~NonMoveable() = default;

  NonMoveable(NonMoveable&&) = delete;
  NonMoveable&& operator=(const NonMoveable&&) = delete;
};
} // namespace runtime::base