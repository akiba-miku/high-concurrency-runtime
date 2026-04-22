#include "runtime/api/models_handler.h"

#include "runtime/http/http_types.h"

namespace runtime::api {

ModelsHandler::ModelsHandler(std::vector<ModelInfo> models)
    : models_(std::move(models)) {}

void ModelsHandler::RegisterRoutes(runtime::http::HttpServer& server) {
    server.Get("/v1/models",
        [this](const runtime::http::HttpRequest& req,
               runtime::http::HttpResponse& resp) {
            Handle(req, resp);
        });
}

void ModelsHandler::Handle(const runtime::http::HttpRequest& /*req*/,
                           runtime::http::HttpResponse& resp) {
    std::string data = "[";
    for (std::size_t i = 0; i < models_.size(); ++i) {
        if (i > 0) data += ',';
        data += R"({"id":")" + models_[i].id +
                R"(","object":"model","owned_by":")" + models_[i].owned_by +
                R"("})";
    }
    data += ']';

    resp.SetStatusCode(runtime::http::StatusCode::Ok);
    resp.SetContentType("application/json");
    resp.SetBody(R"({"object":"list","data":)" + data + '}');
}

}  // namespace runtime::api
