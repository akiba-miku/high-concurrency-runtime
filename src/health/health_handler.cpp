#include "runtime/health/health_handler.h"

#include "runtime/http/http_types.h"

#include <string>
#include <vector>

namespace runtime::health {

void HealthHandler::RegisterRoutes(runtime::http::HttpServer& server) {
    server.Get("/healthz",
        [this](const runtime::http::HttpRequest& req,
               runtime::http::HttpResponse& resp) {
            HandleLiveness(req, resp);
        });

    server.Get("/readyz",
        [this](const runtime::http::HttpRequest& req,
               runtime::http::HttpResponse& resp) {
            HandleReadiness(req, resp);
        });
}

void HealthHandler::HandleLiveness(const runtime::http::HttpRequest&,
                                   runtime::http::HttpResponse& resp) {
    resp.SetStatusCode(runtime::http::StatusCode::Ok);
    resp.SetContentType("application/json");
    resp.SetBody(R"({"status":"ok"})");
}

void HealthHandler::HandleReadiness(const runtime::http::HttpRequest&,
                                    runtime::http::HttpResponse& resp) {
    std::vector<std::string> failures;
    {
        std::lock_guard lock(mutex_);
        for (const auto& check : checks_) {
            bool ok = false;
            try {
                ok = check.fn();
            } catch (...) {
                ok = false;
            }
            if (!ok) failures.push_back(check.name);
        }
    }

    if (failures.empty()) {
        resp.SetStatusCode(runtime::http::StatusCode::Ok);
        resp.SetContentType("application/json");
        resp.SetBody(R"({"status":"ready"})");
        return;
    }

    resp.SetStatusCode(runtime::http::StatusCode::ServiceUnavailable);
    resp.SetContentType("application/json");

    std::string body = R"({"status":"not_ready","failed":[)";
    for (std::size_t i = 0; i < failures.size(); ++i) {
        if (i > 0) body += ",";
        body += "\"" + failures[i] + "\"";
    }
    body += "]}";
    resp.SetBody(std::move(body));
}

}  // namespace runtime::health
