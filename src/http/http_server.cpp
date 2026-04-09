#include "runtime/http/http_server.h"

#include "runtime/log/logger.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string_view>
#include <vector>

namespace runtime::http {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string PercentDecode(std::string_view value) {
    auto from_hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        return -1;
    };

    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const int hi = from_hex(value[i + 1]);
            const int lo = from_hex(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (value[i] == '+') {
            decoded.push_back(' ');
            continue;
        }
        decoded.push_back(value[i]);
    }
    return decoded;
}

std::string JsonEscape(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (const char ch : input) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

std::string DetectMimeType(const std::filesystem::path& path) {
    const std::string ext = ToLower(path.extension().string());
    if (ext == ".html" || ext == ".htm") {
        return "text/html; charset=utf-8";
    }
    if (ext == ".css") {
        return "text/css; charset=utf-8";
    }
    if (ext == ".js") {
        return "application/javascript; charset=utf-8";
    }
    if (ext == ".json") {
        return "application/json; charset=utf-8";
    }
    if (ext == ".txt" || ext == ".log") {
        return "text/plain; charset=utf-8";
    }
    if (ext == ".png") {
        return "image/png";
    }
    if (ext == ".jpg" || ext == ".jpeg") {
        return "image/jpeg";
    }
    if (ext == ".svg") {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

double AgeSeconds(runtime::time::Timestamp now, runtime::time::Timestamp then) {
    return runtime::time::TimeDifference(now, then);
}

bool IsWithinRoot(const std::filesystem::path& root,
                  const std::filesystem::path& candidate) {
    auto root_it = root.begin();
    auto candidate_it = candidate.begin();

    for (; root_it != root.end() && candidate_it != candidate.end(); ++root_it, ++candidate_it) {
        if (*root_it != *candidate_it) {
            return false;
        }
    }

    return root_it == root.end();
}

}  // namespace

std::size_t HttpServer::RouteKeyHash::operator()(const RouteKey& key) const {
    const std::size_t h1 = std::hash<int>{}(static_cast<int>(key.method));
    const std::size_t h2 = std::hash<std::string>{}(key.path);
    return h1 ^ (h2 << 1);
}

HttpServer::HttpServer(runtime::net::EventLoop* loop,
                       const runtime::net::InetAddress& listen_addr,
                       std::string name)
    : server_(loop, listen_addr, name),
      name_(std::move(name)) {
    server_.SetConnectionCallback([this](const TcpConnectionPtr& conn) {
        HandleConnection(conn);
    });
    server_.SetMessageCallback([this](const TcpConnectionPtr& conn,
                                      const std::string& message,
                                      runtime::time::Timestamp receive_time) {
        HandleMessage(conn, message, receive_time);
    });
}

HttpServer::~HttpServer() = default;

void HttpServer::SetThreadNum(int num_threads) {
    server_.SetThreadNum(num_threads);
}

void HttpServer::SetIdleTimeout(std::chrono::milliseconds timeout) {
    idle_timeout_ = timeout;
}

void HttpServer::SetKeepAliveEnabled(bool enabled) {
    keep_alive_enabled_ = enabled;
}

void HttpServer::SetStaticRoot(std::filesystem::path root) {
    static_root_ = std::move(root);
}

void HttpServer::SetStaticUrlPrefix(std::string prefix) {
    if (prefix.empty()) {
        prefix = "/";
    }
    if (prefix.back() != '/') {
        prefix.push_back('/');
    }
    static_url_prefix_ = std::move(prefix);
}

void HttpServer::AddRoute(HttpRequest::Method method,
                          std::string path,
                          HttpHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    routes_[RouteKey{method, std::move(path)}] = std::move(handler);
}

void HttpServer::Get(std::string path, HttpHandler handler) {
    AddRoute(HttpRequest::Method::Get, std::move(path), std::move(handler));
}

void HttpServer::Post(std::string path, HttpHandler handler) {
    AddRoute(HttpRequest::Method::Post, std::move(path), std::move(handler));
}

void HttpServer::Start() {
    if (started_) {
        return;
    }
    started_ = true;
    idle_sweeper_ = std::jthread([this](std::stop_token stop_token) {
        SweepIdleConnections(stop_token);
    });
    server_.Start();
}

void HttpServer::HandleConnection(const TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn->Connected()) {
        ConnectionState state;
        state.connection = conn;
        state.last_active = runtime::time::Timestamp::Now();
        connection_states_[conn->Name()] = std::move(state);
        return;
    }

    connection_states_.erase(conn->Name());
}

void HttpServer::HandleMessage(const TcpConnectionPtr& conn,
                               const std::string& message,
                               runtime::time::Timestamp receive_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    ConnectionState& state = connection_states_[conn->Name()];
    state.connection = conn;
    state.last_active = receive_time;
    state.closing = false;

    if (!state.context.ParseRequest(message, receive_time)) {
        lock.unlock();
        const std::string trace_id = GenerateTraceId();
        HttpResponse response = BuildErrorResponse(
            HttpResponse::StatusCode::_400_BadRequest, true, trace_id, "malformed http request");
        conn->Send(response.ToString());
        conn->Shutdown();
        return;
    }

    while (true) {
        auto it = connection_states_.find(conn->Name());
        if (it == connection_states_.end() || !it->second.context.GotAll()) {
            return;
        }

        const HttpRequest request = it->second.context.Request();
        it->second.context.Reset();
        it->second.last_active = receive_time;
        lock.unlock();

        const std::string trace_id =
            request.GetHeader("x-trace-id").empty() ? GenerateTraceId()
                                                    : request.GetHeader("x-trace-id");
        HttpResponse response(!(keep_alive_enabled_ && request.KeepAlive()));
        response.SetStatusCode(HttpResponse::StatusCode::_200_Ok);
        response.SetStatusMessage("OK");
        response.AddHeader("Server", "runtime-http");
        response.AddHeader("X-Trace-Id", trace_id);

        try {
            DispatchRequest(request, &response, trace_id);
        } catch (const std::exception& ex) {
            response = BuildErrorResponse(HttpResponse::StatusCode::_500_InternalServerError,
                                          true,
                                          trace_id,
                                          ex.what());
        } catch (...) {
            response = BuildErrorResponse(HttpResponse::StatusCode::_500_InternalServerError,
                                          true,
                                          trace_id,
                                          "unexpected server error");
        }

        conn->Send(response.ToString());
        if (response.CloseConnection()) {
            conn->Shutdown();
        }

        lock.lock();
        it = connection_states_.find(conn->Name());
        if (it == connection_states_.end()) {
            return;
        }
        it->second.last_active = runtime::time::Timestamp::Now();
        if (!it->second.context.ParseRequest("", receive_time)) {
            lock.unlock();
            HttpResponse bad = BuildErrorResponse(HttpResponse::StatusCode::_400_BadRequest,
                                                  true,
                                                  GenerateTraceId(),
                                                  "malformed http request");
            conn->Send(bad.ToString());
            conn->Shutdown();
            return;
        }
    }
}

void HttpServer::SweepIdleConnections(std::stop_token stop_token) {
    const auto sweep_interval = std::max(std::chrono::milliseconds(250), idle_timeout_ / 2);

    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(sweep_interval);

        std::vector<std::pair<TcpConnectionPtr, bool>> expired;
        const runtime::time::Timestamp now = runtime::time::Timestamp::Now();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [name, state] : connection_states_) {
                if (state.closing) {
                    continue;
                }
                auto conn = state.connection.lock();
                if (!conn) {
                    continue;
                }
                if (AgeSeconds(now, state.last_active) * 1000.0 <
                    static_cast<double>(idle_timeout_.count())) {
                    continue;
                }
                state.closing = true;
                expired.emplace_back(std::move(conn), state.context.HasBufferedData());
                LOG_INFO() << "closing idle http connection: name=" << name
                           << " timeout_ms=" << idle_timeout_.count();
            }
        }

        for (const auto& [conn, has_pending_request] : expired) {
            if (has_pending_request) {
                const std::string trace_id = GenerateTraceId();
                HttpResponse response = BuildErrorResponse(
                    HttpResponse::StatusCode::_408_RequestTimeout,
                    true,
                    trace_id,
                    "request timed out");
                conn->Send(response.ToString());
            }
            conn->Shutdown();
        }
    }
}

HttpResponse HttpServer::BuildErrorResponse(HttpResponse::StatusCode code,
                                            bool close_connection,
                                            std::string trace_id,
                                            std::string message) const {
    HttpResponse response(close_connection);
    response.SetStatusCode(code);
    response.SetContentType("application/json; charset=utf-8");
    response.AddHeader("Server", "runtime-http");
    response.AddHeader("X-Trace-Id", std::move(trace_id));
    response.SetBody("{\"error\":\"" + JsonEscape(message) + "\"}");
    return response;
}

bool HttpServer::HandleStaticFile(const HttpRequest& request,
                                  HttpResponse* response,
                                  const std::string& trace_id) const {
    if (static_root_.empty()) {
        return false;
    }

    if (request.Path().rfind(static_url_prefix_, 0) != 0) {
        return false;
    }

    if (request.GetMethod() != HttpRequest::Method::Get &&
        request.GetMethod() != HttpRequest::Method::Head) {
        *response = BuildErrorResponse(HttpResponse::StatusCode::_405_MethodNotAllowed,
                                       true,
                                       trace_id,
                                       "static files only support GET and HEAD");
        return true;
    }

    std::string relative = request.Path().substr(static_url_prefix_.size());
    if (relative.empty()) {
        relative = "index.html";
    }
    relative = PercentDecode(relative);

    const std::filesystem::path root = std::filesystem::weakly_canonical(static_root_);
    const std::filesystem::path candidate =
        std::filesystem::weakly_canonical(root / relative);

    if (!IsWithinRoot(root, candidate)) {
        *response = BuildErrorResponse(HttpResponse::StatusCode::_403_Forbidden,
                                       response->CloseConnection(),
                                       trace_id,
                                       "forbidden");
        return true;
    }

    if (!std::filesystem::exists(candidate) || !std::filesystem::is_regular_file(candidate)) {
        *response = BuildErrorResponse(HttpResponse::StatusCode::_404_NotFound,
                                       response->CloseConnection(),
                                       trace_id,
                                       "file not found");
        return true;
    }

    std::ifstream input(candidate, std::ios::binary);
    if (!input) {
        *response = BuildErrorResponse(HttpResponse::StatusCode::_500_InternalServerError,
                                       true,
                                       trace_id,
                                       "failed to open file");
        return true;
    }

    std::ostringstream content;
    content << input.rdbuf();
    response->SetStatusCode(HttpResponse::StatusCode::_200_Ok);
    response->SetStatusMessage("OK");
    response->SetContentType(DetectMimeType(candidate));
    if (request.GetMethod() == HttpRequest::Method::Get) {
        response->SetBody(content.str());
    } else {
        response->SetBody("");
        response->AddHeader("X-Head-Response", "true");
    }
    return true;
}

void HttpServer::DispatchRequest(const HttpRequest& request,
                                 HttpResponse* response,
                                 const std::string& trace_id) const {
    if (HandleStaticFile(request, response, trace_id)) {
        return;
    }

    HttpHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = routes_.find(RouteKey{request.GetMethod(), request.Path()});
        if (it != routes_.end()) {
            handler = it->second;
        }
    }

    if (handler) {
        handler(request, response);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [key, _] : routes_) {
            if (key.path == request.Path()) {
                *response = BuildErrorResponse(HttpResponse::StatusCode::_405_MethodNotAllowed,
                                               true,
                                               trace_id,
                                               "method not allowed");
                return;
            }
        }
    }

    if (request.GetMethod() != HttpRequest::Method::Get &&
        request.GetMethod() != HttpRequest::Method::Post &&
        request.GetMethod() != HttpRequest::Method::Head) {
        *response = BuildErrorResponse(HttpResponse::StatusCode::_405_MethodNotAllowed,
                                       true,
                                       trace_id,
                                       "method not allowed");
        return;
    }

    *response = BuildErrorResponse(HttpResponse::StatusCode::_404_NotFound,
                                   response->CloseConnection(),
                                   trace_id,
                                   "route not found");
}

std::string HttpServer::GenerateTraceId() {
    static std::atomic<std::uint64_t> seq{1};
    static thread_local std::mt19937_64 rng{std::random_device{}()};

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << rng()
        << std::setw(16) << seq.fetch_add(1, std::memory_order_relaxed);
    return oss.str();
}

}  // namespace runtime::http
