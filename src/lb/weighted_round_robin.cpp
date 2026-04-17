#include "runtime/lb/weighted_round_robin.h"

#include <atomic>
#include <limits>

namespace runtime::lb {
runtime::upstream::Backend *WeightedRoundRobinLoadBalancer::Select(
    runtime::upstream::Upstream &upstream) noexcept {
  const auto &backends = upstream.Backends();
  if (backends.empty()) {
    return nullptr;
  }

  std::lock_guard<std::mutex> lk{mutex_};

  // N1: backend list is append-only, so it is sufficient to lazily extend
  // the per-index state vector. Newly added backends start with current = 0.
  if (current_weight_.size() < backends.size()) {
    current_weight_.resize(backends.size(), 0);
  }

  int total_weight = 0;
  int best_weight = std::numeric_limits<int>::max();
  std::size_t winner_idx = backends.size();

  for (std::size_t i = 0; i < backends.size(); ++i) {
    const auto &backend = backends[i];
    if (!backend) {
      continue;
    }

    if (!backend->healthy.load(std::memory_order_relaxed)) {
      // keep unhealthy nodes out of rotation.
      // Resetting avoids stale
      // accumulated weight causing an unfair jump when the node recovers.
      current_weight_[i] = 0;
      continue;
    }

    const int weight = backend->weight > 0 ? backend->weight : 1;
    current_weight_[i] += weight;
    total_weight += weight;

    if (winner_idx == backends.size() || current_weight_[i] > best_weight) {
      best_weight = current_weight_[i];
      winner_idx = i;
    }
  }

  if (winner_idx == backends.size()) {
    return nullptr;
  }

  current_weight_[winner_idx] -= total_weight;
  return backends[winner_idx].get();
}
} // namespace runtime::lb