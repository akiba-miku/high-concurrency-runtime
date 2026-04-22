#include "runtime/metrics/registry.h"

#include <string>

namespace runtime::metrics {

Registry& Registry::Global() {
    static Registry instance;
    return instance;
}

Counter& Registry::GetOrCreateCounter(const std::string& name,
                                      const Labels& labels) {
    {
        std::shared_lock read(mutex_);
        auto it = counters_.find(name);
        if (it != counters_.end()) {
            for (auto& e : it->second) {
                if (e.labels == labels) return *e.metric;
            }
        }
    }
    std::unique_lock write(mutex_);
    auto& vec = counters_[name];
    for (auto& e : vec) {
        if (e.labels == labels) return *e.metric;
    }
    vec.push_back({labels, std::make_unique<Counter>()});
    return *vec.back().metric;
}

Gauge& Registry::GetOrCreateGauge(const std::string& name,
                                   const Labels& labels) {
    {
        std::shared_lock read(mutex_);
        auto it = gauges_.find(name);
        if (it != gauges_.end()) {
            for (auto& e : it->second) {
                if (e.labels == labels) return *e.metric;
            }
        }
    }
    std::unique_lock write(mutex_);
    auto& vec = gauges_[name];
    for (auto& e : vec) {
        if (e.labels == labels) return *e.metric;
    }
    vec.push_back({labels, std::make_unique<Gauge>()});
    return *vec.back().metric;
}

Histogram& Registry::GetOrCreateHistogram(const std::string& name,
                                           std::vector<double> buckets,
                                           const Labels& labels) {
    {
        std::shared_lock read(mutex_);
        auto it = histograms_.find(name);
        if (it != histograms_.end()) {
            for (auto& e : it->second) {
                if (e.labels == labels) return *e.metric;
            }
        }
    }
    std::unique_lock write(mutex_);
    auto& vec = histograms_[name];
    for (auto& e : vec) {
        if (e.labels == labels) return *e.metric;
    }
    vec.push_back({labels, std::make_unique<Histogram>(std::move(buckets))});
    return *vec.back().metric;
}

std::string Registry::SerializeAll() const {
    std::shared_lock read(mutex_);
    std::string out;
    out.reserve(4096);

    for (const auto& [name, entries] : counters_) {
        out += "# TYPE " + name + " counter\n";
        for (const auto& e : entries) {
            out += name + e.labels + " " +
                   std::to_string(e.metric->Value()) + "\n";
        }
    }

    for (const auto& [name, entries] : gauges_) {
        out += "# TYPE " + name + " gauge\n";
        for (const auto& e : entries) {
            out += name + e.labels + " " +
                   std::to_string(e.metric->Value()) + "\n";
        }
    }

    for (const auto& [name, entries] : histograms_) {
        out += "# TYPE " + name + " histogram\n";
        for (const auto& e : entries) {
            out += e.metric->Serialize(name, e.labels);
        }
    }

    return out;
}

}  // namespace runtime::metrics
