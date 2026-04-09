#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/http/http_context.h"
#include "runtime/http/http_request.h"
#include "runtime/http/http_response.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_server.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace runtime::http {

class HttpServer : public runtime::base::NonCopyable {
public:
    using TcpConnectionPtr = runtime::net::TcpServer::TcpConnectionPtr;
    using HttpHandler = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(runtime::net::EventLoop* loop,
               const runtime::net::InetAddress& listen_addr,
               std::string name);
    ~HttpServer();

    void SetThreadNum(int num_threads);
    void SetIdleTimeout(std::chrono::milliseconds timeout);
    void SetKeepAliveEnabled(bool enabled);
    void SetStaticRoot(std::filesystem::path root);
    void SetStaticUrlPrefix(std::string prefix);

    void AddRoute(HttpRequest::Method method, std::string path, HttpHandler handler);
    void Get(std::string path, HttpHandler handler);
    void Post(std::string path, HttpHandler handler);

    void Start();

private:
    struct ConnectionState {
        HttpContext context;
        std::weak_ptr<runtime::net::TcpConnection> connection;
        runtime::time::Timestamp last_active;
        bool closing{false};
    };

    struct RouteKey {
        HttpRequest::Method method;
        std::string path;

        bool operator==(const RouteKey& other) const {
            return method == other.method && path == other.path;
        }
    };

    struct RouteKeyHash {
        std::size_t operator()(const RouteKey& key) const;
    };

    void HandleConnection(const TcpConnectionPtr& conn);
    void HandleMessage(const TcpConnectionPtr& conn,
                       const std::string& message,
                       runtime::time::Timestamp receive_time);
    void SweepIdleConnections(std::stop_token stop_token);

    HttpResponse BuildErrorResponse(HttpResponse::StatusCode code,
                                    bool close_connection,
                                    std::string trace_id,
                                    std::string message) const;
    bool HandleStaticFile(const HttpRequest& request,
                          HttpResponse* response,
                          const std::string& trace_id) const;
    void DispatchRequest(const HttpRequest& request,
                         HttpResponse* response,
                         const std::string& trace_id) const;

    static std::string GenerateTraceId();

private:
    runtime::net::TcpServer server_;
    std::string name_;

    std::chrono::milliseconds idle_timeout_{std::chrono::seconds(30)};
    bool keep_alive_enabled_{true};
    std::filesystem::path static_root_;
    std::string static_url_prefix_{"/static/"};

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ConnectionState> connection_states_;
    std::unordered_map<RouteKey, HttpHandler, RouteKeyHash> routes_;

    std::jthread idle_sweeper_;
    bool started_{false};
};

}  // namespace runtime::http
