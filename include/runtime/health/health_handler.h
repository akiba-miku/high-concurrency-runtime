#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/http/http_request.h"
#include "runtime/http/http_response.h"
#include "runtime/http/http_server.h"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace runtime::health {

// Liveness:  GET /healthz  — always 200 while the process is alive.
// Readiness: GET /readyz   — 200 if all registered checks pass, 503 otherwise.
//
// Usage:
//   HealthHandler health;
//   health.RegisterReadinessCheck("model_loaded", [&engine] {
//       return engine.IsReady();
//   });
//   health.RegisterRoutes(http_server);
class HealthHandler : public runtime::base::NonCopyable {
public:
    using CheckFn = std::function<bool()>;

    // Register a named readiness check. All checks must pass for /readyz → 200.
    void RegisterReadinessCheck(std::string name, CheckFn fn) {
        std::lock_guard lock(mutex_);
        checks_.push_back({std::move(name), std::move(fn)});
    }

    // Mount routes on the given HttpServer.
    void RegisterRoutes(runtime::http::HttpServer& server);

private:
    void HandleLiveness(const runtime::http::HttpRequest& req,
                        runtime::http::HttpResponse& resp);

    void HandleReadiness(const runtime::http::HttpRequest& req,
                         runtime::http::HttpResponse& resp);

    struct Check {
        std::string name;
        CheckFn fn;
    };

    mutable std::mutex mutex_;
    std::vector<Check> checks_;
};

}  // namespace runtime::health
