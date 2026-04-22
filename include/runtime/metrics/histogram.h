#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace runtime::metrics {

// Fixed-bucket histogram. Observations are O(log n) over bucket count.
class Histogram {
public:
    explicit Histogram(std::vector<double> upper_bounds)
        : bounds_(std::move(upper_bounds))
        , counts_(bounds_.size() + 1, 0) {}

    void Observe(double value) {
        std::lock_guard lock(mutex_);
        for (std::size_t i = 0; i < bounds_.size(); ++i) {
            if (value <= bounds_[i]) {
                ++counts_[i];
                ++sum_count_;
                sum_ += value;
                return;
            }
        }
        ++counts_.back();
        ++sum_count_;
        sum_ += value;
    }

    // Prometheus text format for one histogram metric.
    // labels should be empty or formatted as `{key="val",...}`.
    std::string Serialize(std::string_view name, std::string_view labels = "") const {
        std::lock_guard lock(mutex_);
        std::string lbl(labels);
        std::string n(name);
        std::string out;

        std::size_t cumulative = 0;
        for (std::size_t i = 0; i < bounds_.size(); ++i) {
            cumulative += counts_[i];
            out += n + "_bucket{le=\"" + std::to_string(bounds_[i]);
            if (!lbl.empty()) out += "\"," + lbl.substr(1);  // merge labels
            else out += "\"}";
            out += " " + std::to_string(cumulative) + "\n";
        }
        cumulative += counts_.back();
        out += n + "_bucket{le=\"+Inf\"";
        if (!lbl.empty()) out += "," + lbl.substr(1);
        else out += "}";
        out += " " + std::to_string(cumulative) + "\n";
        out += n + "_sum" + lbl + " " + std::to_string(sum_) + "\n";
        out += n + "_count" + lbl + " " + std::to_string(sum_count_) + "\n";
        return out;
    }

    static std::vector<double> DefaultLatencyBuckets() {
        return {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
    }

private:
    mutable std::mutex mutex_;
    std::vector<double> bounds_;
    std::vector<std::size_t> counts_;
    double sum_{0.0};
    std::size_t sum_count_{0};
};

}  // namespace runtime::metrics
