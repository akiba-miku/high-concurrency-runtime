#pragma once

#include "runtime/base/noncopyable.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace runtime::upstream {

// Backend represents one physical backend server instance.
//
// Design note: 
// Backend is intentionally non-copyable because it contains some
// atomic members.  
// Upstream holds backends via unique_ptr so that callers can
// safely keep a raw Backend* across load-balancer selections without the
// pointer being invalidated by vector reallocations.
struct Backend : public runtime::base::NonCopyable {
    std::string host;
    uint16_t    port{0};

    // Static weight assigned at registration time.
    // Used by WeightedRoundRobinLB (Phase N3); ignored by plain RoundRobinLB.
    int weight{1};

    // Set to false by HealthChecker when probes fail; restored on recovery.
    // LoadBalancer implementations must skip backends where healthy == false.
    std::atomic<bool> healthy{true};

    // Incremented atomically when a proxy session begins forwarding to this
    // backend; decremented when the session ends.  Used by LeastConnectionsLB.
    std::atomic<int> active_requests{0};

    // Passive health counter: incremented by ReverseProxy on connection/
    // read errors, reset to 0 on success.  HealthChecker uses this to decide
    // when to mark the backend unhealthy without waiting for an active probe.
    std::atomic<int> fail_count{0};

    // Epoch-microseconds of the last active health probe (0 = never probed).
    // Written by HealthChecker, read for logging; not used in hot path.
    int64_t last_check_us{0};

    Backend() = default;
    Backend(std::string h, uint16_t p, int w = 1)
        : host(std::move(h)), port(p), weight(w) {}

    std::string Address() const { return host + ":" + std::to_string(port); }
};

}  // namespace runtime::upstream

