#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/metrics/counter.h"
#include "runtime/metrics/gauge.h"
#include "runtime/metrics/histogram.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace runtime::metrics {

using Labels = std::string;  // formatted: '{key="v",...}' or ""

// Global singleton metric registry. Thread-safe reads via shared_mutex.
class Registry : public runtime::base::NonCopyable {
public:
    static Registry& Global();

    Counter& GetOrCreateCounter(const std::string& name,
                                const Labels& labels = "");

    Gauge& GetOrCreateGauge(const std::string& name,
                            const Labels& labels = "");

    Histogram& GetOrCreateHistogram(const std::string& name,
                                    std::vector<double> buckets = Histogram::DefaultLatencyBuckets(),
                                    const Labels& labels = "");

    // Serializes all metrics in Prometheus text exposition format.
    std::string SerializeAll() const;

private:
    Registry() = default;

    struct CounterEntry { std::string labels; std::unique_ptr<Counter> metric; };
    struct GaugeEntry   { std::string labels; std::unique_ptr<Gauge>   metric; };
    struct HistogramEntry { std::string labels; std::unique_ptr<Histogram> metric; };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<CounterEntry>>   counters_;
    std::unordered_map<std::string, std::vector<GaugeEntry>>     gauges_;
    std::unordered_map<std::string, std::vector<HistogramEntry>> histograms_;
};

}  // namespace runtime::metrics
