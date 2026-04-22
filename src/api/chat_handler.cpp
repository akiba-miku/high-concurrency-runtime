#include "runtime/api/chat_handler.h"

#include "runtime/api/sse_writer.h"
#include "runtime/http/http_types.h"
#include "runtime/log/logger.h"

#include <future>
#include <mutex>
#include <string>

namespace runtime::api {

ChatHandler::ChatHandler(
    std::shared_ptr<runtime::inference::BatchScheduler> scheduler,
    std::string model_name)
    : scheduler_(std::move(scheduler))
    , model_name_(std::move(model_name)) {}

void ChatHandler::RegisterRoutes(runtime::http::HttpServer& server) {
    server.Post("/v1/chat/completions",
        [this](const runtime::http::HttpRequest& req,
               runtime::http::HttpResponse& resp) {
            Handle(req, resp);
        });
}

void ChatHandler::Handle(const runtime::http::HttpRequest& req,
                         runtime::http::HttpResponse& resp) {
    runtime::inference::InferenceRequest inf_req;
    std::string error;

    if (!ParseRequest(req, inf_req, error)) {
        resp.SetStatusCode(runtime::http::StatusCode::BadRequest);
        resp.SetContentType("application/json");
        resp.SetBody("{\"error\":\"" + error + "\"}");
        return;
    }

    if (!scheduler_ || !scheduler_->IsReady()) {
        resp.SetStatusCode(runtime::http::StatusCode::ServiceUnavailable);
        resp.SetContentType("application/json");
        resp.SetBody(R"({"error":"model not ready"})");
        return;
    }

    if (inf_req.stream_) {
        HandleStream(req, resp);
    } else {
        HandleBuffered(req, resp);
    }
}

void ChatHandler::HandleStream(const runtime::http::HttpRequest& req,
                               runtime::http::HttpResponse& resp) {
    runtime::inference::InferenceRequest inf_req;
    std::string error;
    if (!ParseRequest(req, inf_req, error)) {
        resp.SetStatusCode(runtime::http::StatusCode::BadRequest);
        return;
    }

    resp.SetStatusCode(runtime::http::StatusCode::Ok);
    resp.AddHeader("Content-Type", "text/event-stream");
    resp.AddHeader("Cache-Control", "no-cache");
    resp.SetCloseConnection(false);

    const auto conn = req.Connection();
    const auto loop = req.IoLoop();
    const std::string model = model_name_;

    inf_req.io_loop_ = loop;
    inf_req.token_cb_ = [conn, model](std::string_view token) {
        SseWriter::WriteToken(conn, token, model);
    };
    inf_req.done_cb_ = [conn](std::string_view /*reason*/) {
        SseWriter::WriteDone(conn);
    };

    scheduler_->Submit(std::move(inf_req));
}

void ChatHandler::HandleBuffered(const runtime::http::HttpRequest& req,
                                 runtime::http::HttpResponse& resp) {
    runtime::inference::InferenceRequest inf_req;
    std::string error;
    if (!ParseRequest(req, inf_req, error)) {
        resp.SetStatusCode(runtime::http::StatusCode::BadRequest);
        return;
    }

    auto promise   = std::make_shared<std::promise<std::string>>();
    auto future    = promise->get_future();
    auto acc       = std::make_shared<std::mutex>();
    auto full_text = std::make_shared<std::string>();

    inf_req.stream_ = false;
    inf_req.token_cb_ = [acc, full_text](std::string_view token) {
        std::lock_guard lock(*acc);
        *full_text += token;
    };
    inf_req.done_cb_ = [promise, full_text](std::string_view /*reason*/) {
        try { promise->set_value(*full_text); } catch (...) {}
    };

    scheduler_->Submit(std::move(inf_req));

    const std::string text = future.get();

    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        if (c == '"')       escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\n') escaped += "\\n";
        else                escaped += c;
    }

    const std::string body =
        R"({"object":"chat.completion","model":")" + model_name_ +
        R"(","choices":[{"message":{"role":"assistant","content":")" +
        escaped +
        R"("},"index":0,"finish_reason":"stop"}]})";

    resp.SetStatusCode(runtime::http::StatusCode::Ok);
    resp.SetContentType("application/json");
    resp.SetBody(body);
}

bool ChatHandler::ParseRequest(const runtime::http::HttpRequest& req,
                               runtime::inference::InferenceRequest& out,
                               std::string& error_msg) {
    const std::string& body = req.Body();
    if (body.empty()) {
        error_msg = "empty body";
        return false;
    }

    const std::string role_key    = "\"role\"";
    const std::string content_key = "\"content\"";
    std::string prompt;

    std::size_t pos = 0;
    while (pos < body.size()) {
        auto rk = body.find(role_key, pos);
        if (rk == std::string::npos) break;
        auto colon = body.find(':', rk + role_key.size());
        if (colon == std::string::npos) break;
        auto q1 = body.find('"', colon + 1);
        if (q1 == std::string::npos) break;
        auto q2 = body.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string role = body.substr(q1 + 1, q2 - q1 - 1);

        auto ck = body.find(content_key, q2);
        if (ck == std::string::npos) break;
        auto ccolon = body.find(':', ck + content_key.size());
        if (ccolon == std::string::npos) break;
        auto cq1 = body.find('"', ccolon + 1);
        if (cq1 == std::string::npos) break;
        auto cq2 = body.find('"', cq1 + 1);
        if (cq2 == std::string::npos) break;

        if (role == "user") prompt = body.substr(cq1 + 1, cq2 - cq1 - 1);
        pos = cq2 + 1;
    }

    if (prompt.empty()) {
        error_msg = "no user message found";
        return false;
    }
    out.prompt_ = std::move(prompt);

    auto stream_pos = body.find("\"stream\"");
    if (stream_pos != std::string::npos) {
        auto colon = body.find(':', stream_pos + 8);
        if (colon != std::string::npos) {
            auto val = body.find_first_not_of(" \t", colon + 1);
            out.stream_ = (val != std::string::npos && body[val] == 't');
        }
    }

    auto mt = body.find("\"max_tokens\"");
    if (mt != std::string::npos) {
        auto colon = body.find(':', mt + 12);
        if (colon != std::string::npos) {
            try { out.max_new_tokens_ = std::stoi(body.substr(colon + 1)); }
            catch (...) {}
        }
    }

    auto temp = body.find("\"temperature\"");
    if (temp != std::string::npos) {
        auto colon = body.find(':', temp + 13);
        if (colon != std::string::npos) {
            try { out.temperature_ = std::stof(body.substr(colon + 1)); }
            catch (...) {}
        }
    }

    return true;
}

}  // namespace runtime::api
