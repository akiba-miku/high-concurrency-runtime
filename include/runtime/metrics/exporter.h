#pragma once

#include "runtime/metrics/registry.h"

#include <string_view>

namespace runtime::metrics {

// Renders the registry in Prometheus text format for /metrics handlers.
class Exporter {
public:
    explicit Exporter(Registry& reg = Registry::Global()) : registry_(reg) {}

    std::string Render() const { return registry_.SerializeAll(); }

    static constexpr std::string_view ContentType() {
        return "text/plain; version=0.0.4; charset=utf-8";
    }

private:
    Registry& registry_;
};

}  // namespace runtime::metrics
