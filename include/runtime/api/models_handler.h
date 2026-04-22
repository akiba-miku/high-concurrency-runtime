#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/http/http_request.h"
#include "runtime/http/http_response.h"
#include "runtime/http/http_server.h"

#include <string>
#include <vector>

namespace runtime::api {

// Handles GET /v1/models — returns a static list of available models.
class ModelsHandler : public runtime::base::NonCopyable {
public:
    struct ModelInfo {
        std::string id;
        std::string owned_by{"local"};
    };

    explicit ModelsHandler(std::vector<ModelInfo> models);

    void RegisterRoutes(runtime::http::HttpServer& server);

private:
    void Handle(const runtime::http::HttpRequest& req,
                runtime::http::HttpResponse& resp);

    std::vector<ModelInfo> models_;
};

}  // namespace runtime::api
