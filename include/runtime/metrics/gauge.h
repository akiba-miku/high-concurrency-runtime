#pragma once

#include <atomic>

namespace runtime::metrics {

// Instantaneous value that can go up or down. Thread-safe via atomic CAS.
class Gauge {
public:
    void Set(double v) { value_.store(v, std::memory_order_release); }

    void Inc(double delta = 1.0) {
        double cur = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(
                   cur, cur + delta,
                   std::memory_order_release,
                   std::memory_order_relaxed)) {}
    }

    void Dec(double delta = 1.0) { Inc(-delta); }

    double Value() const { return value_.load(std::memory_order_acquire); }

private:
    std::atomic<double> value_{0.0};
};

}  // namespace runtime::metrics
