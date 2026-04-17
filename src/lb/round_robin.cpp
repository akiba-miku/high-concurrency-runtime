#include "runtime/lb/round_robin.h"

#include <vector>

namespace runtime::lb {

runtime::upstream::Backend* RoundRobinLoadBalancer::Select(runtime::upstream::Upstream& upstream) noexcept {
    const auto& backends = upstream.Backends();
    if (backends.empty()) {
        return nullptr;
    }

    std::vector<runtime::upstream::Backend*> healthy;
    healthy.reserve(backends.size());

    for (const auto& b : backends) {
        if (b->healthy.load(std::memory_order_relaxed)) {
            healthy.push_back(b.get());
        }
    }

    if (healthy.empty()) {
        return nullptr;
    }

    uint64_t idx = counter_.fetch_add(1, std::memory_order_relaxed);
    return healthy[idx % healthy.size()];
}

}  // namespace runtime::lb
