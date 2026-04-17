#pragma once

#include "runtime/lb/load_balancer.h"
#include "runtime/upstream/backend.h"
#include "runtime/upstream/upstream.h"

#include <mutex>
#include <vector>

namespace runtime::lb {

// WeightedRoundRobinLoadBalancer implements nginx-style smooth weighted
// round robin.
//
// State:
// - current_weights_[i] stores the dynamic current_weight for backend i.
// - The vector is lazily grown to match upstream.Backends().size.
// - In N1 the backend list is append-only, so index-based state is stable
//
// Concurrency:
// - Select() mutates current_weights_, so a mutex is used to serialize access
// - This keeps the implementation simple and correct for concurrent workers.
class WeightedRoundRobinLoadBalancer : public runtime::lb::LoadBalancer {
public:
  runtime::upstream::Backend* Select(runtime::upstream::Upstream& upstream) noexcept;

private:
  std::mutex mutex_;
  std::vector<int> current_weight_;
};
} // namespace runtime::lb