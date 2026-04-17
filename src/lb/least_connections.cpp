#include "runtime/lb/least_connections.h"

#include <atomic>
#include <limits>

namespace runtime::lb {

runtime::upstream::Backend* LeastConnectionsLoadBalancer::Select(
    runtime::upstream::Upstream& upstream) noexcept {
    const auto& backends = upstream.Backends();
    if (backends.empty()) {
        return nullptr;
    }

    runtime::upstream::Backend* best = nullptr;
    int best_active = std::numeric_limits<int>::max();

    for (const auto& backend : backends) {
        if (!backend) {
            continue;
        }

        if (!backend->healthy.load(std::memory_order_relaxed)) {
            continue;
        }

        const int active =
            backend->active_requests.load(std::memory_order_relaxed);

        if (best == nullptr || active < best_active) {
            best = backend.get();
            best_active = active;
        }
    }
    return best;
}

}  // namespace runtime::lb