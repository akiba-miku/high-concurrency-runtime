#include <gtest/gtest.h>

#include "runtime/http/http_server.h"
#include "runtime/lb/round_robin.h"
#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/net_utils.h"
#include "runtime/proxy/reverse_proxy.h"
#include "runtime/registry/service_registry.h"
#include "runtime/upstream/upstream.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using runtime::http::HttpServer;
using runtime::lb::RoundRobinLoadBalancer;
using runtime::net::EventLoop;
using runtime::net::InetAddress;
using runtime::net::MakeIPv4Address;
using runtime::proxy::ReverseProxy;
using runtime::registry::ServiceRegistry;
using runtime::upstream::Upstream;

namespace {

// Reserves an ephemeral loopback port (same helper as test_tcp_server.cpp).
std::uint16_t ReserveLoopbackPort() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t len = sizeof(addr);
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    const std::uint16_t port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

// Sends all bytes in payload over fd.
void WriteAll(int fd, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        ssize_t n = ::write(fd, payload.data() + sent, payload.size() - sent);
        ASSERT_GT(n, 0);
        sent += static_cast<std::size_t>(n);
    }
}

// Reads until EOF and returns all received bytes.
std::string ReadUntilEof(int fd) {
    std::string result;
    char buf[4096];
    while (true) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        result.append(buf, static_cast<std::size_t>(n));
    }
    return result;
}

// ---------------------------------------------------------------------------
// Proxy smoke-test: gateway → backend, verifies response piping.
// ---------------------------------------------------------------------------
TEST(ReverseProxyTest, ProxiesRequestToBackendAndPipesResponse) {
    const std::uint16_t backend_port = ReserveLoopbackPort();
    const std::uint16_t gateway_port = ReserveLoopbackPort();

    // --- Backend server -------------------------------------------------------
    std::promise<EventLoop*> backend_ready;
    std::thread backend_thread([&] {
        EventLoop loop;
        HttpServer server(&loop, InetAddress(backend_port, "127.0.0.1"), "backend");
        server.Get("/hello", [](const runtime::http::HttpRequest&,
                                runtime::http::HttpResponse& resp) {
            resp.SetStatusCode(runtime::http::StatusCode::Ok);
            resp.SetContentType("text/plain");
            resp.SetBody("Hello from backend");
        });
        server.Start();
        backend_ready.set_value(&loop);
        loop.Loop();
    });

    EventLoop* backend_loop = backend_ready.get_future().get();

    // --- Gateway server -------------------------------------------------------
    ServiceRegistry registry;
    Upstream us("backend_service");
    us.AddBackend("127.0.0.1", backend_port);
    registry.Register("backend_service", std::move(us));

    RoundRobinLoadBalancer lb;
    auto proxy = std::make_shared<ReverseProxy>(
        &registry, &lb, "backend_service", /*timeout=*/3.0);

    std::promise<EventLoop*> gateway_ready;
    std::thread gateway_thread([&] {
        EventLoop loop;
        HttpServer server(&loop, InetAddress(gateway_port, "127.0.0.1"), "gateway");
        server.Proxy("/hello", [proxy](runtime::http::HttpRequest req,
                                       HttpServer::TcpConnectionPtr conn) {
            proxy->Handle(std::move(req), conn);
        });
        server.Start();
        gateway_ready.set_value(&loop);
        loop.Loop();
    });

    EventLoop* gateway_loop = gateway_ready.get_future().get();

    // Give the listeners a moment to be fully ready.
    std::this_thread::sleep_for(50ms);

    // --- Test client ----------------------------------------------------------
    std::string response;
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(fd, 0);
        sockaddr_in addr = MakeIPv4Address("127.0.0.1", gateway_port);
        ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

        const std::string req =
            "GET /hello HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n\r\n";
        WriteAll(fd, req);
        response = ReadUntilEof(fd);
        ::close(fd);
    }

    // --- Verify ---------------------------------------------------------------
    EXPECT_NE(response.find("200"), std::string::npos)
        << "expected HTTP 200 in response; got:\n" << response;
    EXPECT_NE(response.find("Hello from backend"), std::string::npos)
        << "expected backend body in response; got:\n" << response;

    // --- Teardown -------------------------------------------------------------
    gateway_loop->Quit();
    gateway_thread.join();
    backend_loop->Quit();
    backend_thread.join();
}

// ---------------------------------------------------------------------------
// 502 when no upstream is registered.
// ---------------------------------------------------------------------------
TEST(ReverseProxyTest, Returns502WhenNoUpstream) {
    const std::uint16_t gateway_port = ReserveLoopbackPort();

    ServiceRegistry registry;  // empty — no upstream registered
    RoundRobinLoadBalancer lb;
    auto proxy = std::make_shared<ReverseProxy>(
        &registry, &lb, "missing_service", /*timeout=*/3.0);

    std::promise<EventLoop*> ready;
    std::thread gw_thread([&] {
        EventLoop loop;
        HttpServer server(&loop, InetAddress(gateway_port, "127.0.0.1"), "gw");
        server.Proxy("/api", [proxy](runtime::http::HttpRequest req,
                                      HttpServer::TcpConnectionPtr conn) {
            proxy->Handle(std::move(req), conn);
        });
        server.Start();
        ready.set_value(&loop);
        loop.Loop();
    });

    EventLoop* loop = ready.get_future().get();
    std::this_thread::sleep_for(50ms);

    std::string response;
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(fd, 0);
        sockaddr_in addr = MakeIPv4Address("127.0.0.1", gateway_port);
        ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

        const std::string req =
            "GET /api/users HTTP/1.1\r\n"
            "Host: localhost\r\nConnection: close\r\n\r\n";
        WriteAll(fd, req);
        response = ReadUntilEof(fd);
        ::close(fd);
    }

    EXPECT_NE(response.find("502"), std::string::npos)
        << "expected 502; got:\n" << response;

    loop->Quit();
    gw_thread.join();
}

// ---------------------------------------------------------------------------
// 502 when all backends are unhealthy.
// ---------------------------------------------------------------------------
TEST(ReverseProxyTest, Returns502WhenAllBackendsUnhealthy) {
    const std::uint16_t gateway_port = ReserveLoopbackPort();

    ServiceRegistry registry;
    Upstream us("svc");
    us.AddBackend("127.0.0.1", 19999);  // nothing listening here
    us.Backends()[0]->healthy.store(false, std::memory_order_relaxed);
    registry.Register("svc", std::move(us));

    RoundRobinLoadBalancer lb;
    auto proxy = std::make_shared<ReverseProxy>(
        &registry, &lb, "svc", /*timeout=*/3.0);

    std::promise<EventLoop*> ready;
    std::thread gw_thread([&] {
        EventLoop loop;
        HttpServer server(&loop, InetAddress(gateway_port, "127.0.0.1"), "gw");
        server.Proxy("/", [proxy](runtime::http::HttpRequest req,
                                   HttpServer::TcpConnectionPtr conn) {
            proxy->Handle(std::move(req), conn);
        });
        server.Start();
        ready.set_value(&loop);
        loop.Loop();
    });

    EventLoop* loop = ready.get_future().get();
    std::this_thread::sleep_for(50ms);

    std::string response;
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(fd, 0);
        sockaddr_in addr = MakeIPv4Address("127.0.0.1", gateway_port);
        ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

        const std::string req =
            "GET /health HTTP/1.1\r\n"
            "Host: localhost\r\nConnection: close\r\n\r\n";
        WriteAll(fd, req);
        response = ReadUntilEof(fd);
        ::close(fd);
    }

    EXPECT_NE(response.find("502"), std::string::npos)
        << "expected 502; got:\n" << response;

    loop->Quit();
    gw_thread.join();
}

}  // namespace
