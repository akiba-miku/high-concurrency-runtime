#pragma once

#include "runtime/lb/load_balancer.h"
#include "runtime/upstream/upstream.h"

namespace runtime::lb {

// Selects the healthy backend with the fewest active_requests.
//
// Notes:
// - active_requests is read with memory_order_relaxed: only a best-effort
//   load estimate is needed; strict ordering is not required.
// - Ties are broken by the first backend encountered.
class LeastConnectionsLoadBalancer : public runtime::lb::LoadBalancer {
public:
  runtime::upstream::Backend *Select(
      runtime::upstream::Upstream &upstream) noexcept override;
};
} // namespace runtime::lb