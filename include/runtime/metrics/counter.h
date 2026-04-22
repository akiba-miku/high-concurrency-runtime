#pragma once

#include <atomic>
#include <bit>
#include <string>
#include <string_view>

namespace runtime::metrics {

// Monotonically increasing counter. Thread-safe via atomic CAS.
class Counter {
public:
    void Inc(double delta = 1.0) {
        double current = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(
                   current, current + delta,
                   std::memory_order_release,
                   std::memory_order_relaxed)) {}
    }

    double Value() const { return value_.load(std::memory_order_acquire); }

    void Reset() { value_.store(0.0, std::memory_order_release); }

private:
    std::atomic<double> value_{0.0};
};

}  // namespace runtime::metrics
