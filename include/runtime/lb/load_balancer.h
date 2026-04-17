#pragma once

#include "runtime/upstream/backend.h"
#include "runtime/upstream/upstream.h"

namespace runtime::lb {

// LoadBalancer selects one backend from an upstream for each incoming request
//
// Thread-safety:
// Implementations must be safe for concurrent Select() calls from multiple
// worker threads without relying on external synchronization
//
// ownership:
// The returned Bakcend* is owned by the given Upstream and must not be free
// by the caller. The pointer remains valid as long as that Backend is still
// present in the Upstream
class LoadBalancer {
public:
  virtual~LoadBalancer() = default;

  // Returns a poniter to a selected healthy backend, or nullptr if no
  // selectable backend exists
  virtual runtime::upstream::Backend* Select(runtime::upstream::Upstream& upstream) noexcept = 0;
};

} // namespace runtime::lb
