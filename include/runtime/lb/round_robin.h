#pragma once

#include "runtime/lb/load_balancer.h"

#include <atomic>
#include <cstdint>

namespace runtime::lb {

// RoundRobinLB distributes requests evenly across all healthy backends.
//
// Implementation:
// - Each Select() call scans the upstream and considers only healthy backends.
// - A shared atomic counter is incremented per call.
// - The selected backend is determined by counter % healthy_count.
//
// Concurrency:
// - Safe for concurrent Select() calls from multiple worker threads.
// - No external lock is required for round-robin state management.
//
// Trade-off:
// - Each Select() call performs an O(n) scan over backends.
// - When the healthy backend set changes, exact round-robin continuity is not
//   preserved. This is acceptable for this project because upstream sizes are
//   small and backend health changes should be reflected quickly.
class RoundRobinLoadBalancer : public LoadBalancer {
public:
  runtime::upstream::Backend* Select(runtime::upstream::Upstream& upstream) noexcept override;

private:
  std::atomic<uint64_t> counter_{0L};
};
} // namespace runtime::lb