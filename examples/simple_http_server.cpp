#include "runtime/log/logger.h"
#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_server.h"

#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

constexpr char kHttpResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Server: runtime-bench\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 3\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "OK\n";

constexpr char kRequestDelimiter[] = "\r\n\r\n";
constexpr std::size_t kMaxPendingRequestBytes = 64 * 1024;

struct SharedHttpState {
    std::mutex mutex;
    std::unordered_map<std::string, std::string> pending_requests;
};

}  // namespace

int main() {
    auto& logger = runtime::log::Logger::Instance();
    logger.Init("simple_http_server.log", runtime::log::LogLevel::INFO);

    const char* host_env = std::getenv("HOST");
    const char* port_env = std::getenv("PORT");
    const std::string host = host_env == nullptr ? "127.0.0.1" : host_env;
    const std::uint16_t port =
        static_cast<std::uint16_t>(port_env == nullptr ? 18081 : std::stoi(port_env));

    runtime::net::EventLoop main_loop;
    runtime::net::InetAddress listen_addr(port, host);
    runtime::net::TcpServer server(&main_loop, listen_addr, "SimpleHttpServer");

    SharedHttpState state;

    unsigned int thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) {
        thread_count = 4;
    }
    server.SetThreadNum(static_cast<int>(thread_count));

    server.SetConnectionCallback(
        [&state](const std::shared_ptr<runtime::net::TcpConnection>& conn) {
            LOG_INFO() << "[http-connection] " << conn->Name()
                       << " local=" << conn->LocalAddress().ToIpPort()
                       << " peer=" << conn->PeerAddress().ToIpPort()
                       << " connected=" << conn->Connected();

            if (!conn->Connected()) {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.pending_requests.erase(conn->Name());
            }
        });

    server.SetMessageCallback(
        [&state](const std::shared_ptr<runtime::net::TcpConnection>& conn,
                 const std::string& message,
                 runtime::time::Timestamp) {
            std::size_t complete_requests = 0;
            bool close_connection = false;

            {
                std::lock_guard<std::mutex> lock(state.mutex);
                std::string& pending = state.pending_requests[conn->Name()];
                pending.append(message);

                std::size_t pos = pending.find(kRequestDelimiter);
                while (pos != std::string::npos) {
                    ++complete_requests;
                    pending.erase(0, pos + 4);
                    pos = pending.find(kRequestDelimiter);
                }

                if (pending.size() > kMaxPendingRequestBytes) {
                    pending.clear();
                    close_connection = true;
                }
            }

            for (std::size_t i = 0; i < complete_requests; ++i) {
                conn->Send(kHttpResponse);
            }

            if (close_connection) {
                LOG_WARN() << "closing oversized http request buffer for "
                           << conn->Name();
                conn->Shutdown();
            }
        });

    server.SetWriteCompleteCallback(
        [](const std::shared_ptr<runtime::net::TcpConnection>& conn) {
            LOG_DEBUG() << "[http-write-complete] " << conn->Name();
        });

    server.Start();

    LOG_INFO() << "simple http server listening on " << host << ':' << port
               << " with " << thread_count << " io threads";
    main_loop.Loop();
    return 0;
}
