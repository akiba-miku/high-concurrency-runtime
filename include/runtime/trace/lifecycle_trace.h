#pragma once

#include "runtime/trace/trace_id.h"
#include "runtime/time/timestamp.h"
#include "runtime/log/logger.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace runtime::trace {

// RAII span that records elapsed time on destruction.
// Logs a structured line: [TRACE] <trace_id> <name> <elapsed_us>us
//
// Usage:
//   {
//     LifecycleTrace t("handle_request", id);
//     t.AddEvent("parsed", "ok");
//     // ... work ...
//   }  // logs on exit
class LifecycleTrace {
public:
    explicit LifecycleTrace(std::string name,
                            TraceId id = TraceId::Generate())
        : name_(std::move(name))
        , id_(id)
        , start_(std::chrono::steady_clock::now()) {}

    ~LifecycleTrace() {
        auto end = std::chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      end - start_).count();
        LOG_INFO() << "[TRACE] " << id_.ToHex()
                   << " " << name_
                   << " elapsed=" << us << "us";
    }

    LifecycleTrace(const LifecycleTrace&) = delete;
    LifecycleTrace& operator=(const LifecycleTrace&) = delete;
    LifecycleTrace(LifecycleTrace&&) = delete;
    LifecycleTrace& operator=(LifecycleTrace&&) = delete;

    // Record a key/value annotation (logged on destruction).
    void AddEvent(std::string_view key, std::string_view value) {
        events_.emplace_back(std::string(key), std::string(value));
    }

    TraceId Id() const noexcept { return id_; }
    const std::string& Name() const noexcept { return name_; }

    std::chrono::microseconds Elapsed() const noexcept {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start_);
    }

private:
    std::string name_;
    TraceId id_;
    std::chrono::steady_clock::time_point start_;
    std::vector<std::pair<std::string, std::string>> events_;
};

}  // namespace runtime::trace
