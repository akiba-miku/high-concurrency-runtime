#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/http/http_request.h"
#include "runtime/http/http_response.h"
#include "runtime/http/http_server.h"
#include "runtime/inference/batch_scheduler.h"

#include <memory>
#include <string>

namespace runtime::api {

// Handles POST /v1/chat/completions (OpenAI-compatible).
// Supports stream=true (SSE) and stream=false (buffered JSON).
class ChatHandler : public runtime::base::NonCopyable {
public:
    explicit ChatHandler(
        std::shared_ptr<runtime::inference::BatchScheduler> scheduler,
        std::string model_name = "llama3");

    void RegisterRoutes(runtime::http::HttpServer& server);

private:
    void Handle(const runtime::http::HttpRequest& req,
                runtime::http::HttpResponse& resp);

    void HandleStream(const runtime::http::HttpRequest& req,
                      runtime::http::HttpResponse& resp);

    void HandleBuffered(const runtime::http::HttpRequest& req,
                        runtime::http::HttpResponse& resp);

    bool ParseRequest(const runtime::http::HttpRequest& req,
                      runtime::inference::InferenceRequest& out,
                      std::string& error_msg);

    std::shared_ptr<runtime::inference::BatchScheduler> scheduler_;
    std::string model_name_;
};

}  // namespace runtime::api
