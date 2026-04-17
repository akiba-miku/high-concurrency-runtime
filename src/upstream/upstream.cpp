#include "runtime/upstream/upstream.h"

#include <memory>
#include <utility>

namespace runtime::upstream {

bool Upstream::AddBackend(std::unique_ptr<Backend> backend) {
    if (!backend || backend->port == 0) {
        return false;
    }

    if (backend->weight <= 0) {
        backend->weight = 1;
    }

    backends_.push_back(std::move(backend));
    return true;
}

bool Upstream::AddBackend(std::string host, uint16_t port, int weight) {
    if (host.empty() || port == 0) {
        return false;
    }

    if (weight <= 0) {
        weight = 1;
    }

    backends_.push_back(
        std::make_unique<Backend>(std::move(host), port, weight));
    return true;
}

int Upstream::HealthyCount() const noexcept {
    int count = 0;
    for (const auto& backend : backends_) {
        if (backend && backend->healthy) {
            ++count;
        }
    }
    return count;
}

}  // namespace runtime::upstream