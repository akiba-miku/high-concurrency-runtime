#include "runtime/proxy/reverse_proxy.h"

#include "runtime/log/logger.h"
#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_client.h"
#include "runtime/proxy/proxy_context.h"
#include "runtime/upstream/upstream.h"

#include <string>

namespace runtime::proxy {

ReverseProxy::ReverseProxy(runtime::registry::ServiceRegistry* registry,
                           runtime::lb::LoadBalancer* lb,
                           std::string upstream_name,
                           double connect_timeout_sec)
    : registry_(registry),
      lb_(lb),
      upstream_name_(std::move(upstream_name)),
      connect_timeout_sec_(connect_timeout_sec) {}

void ReverseProxy::Handle(runtime::http::HttpRequest req,
                          TcpConnectionPtr client_conn) {
  runtime::upstream::Upstream* upstream = registry_->Get(upstream_name_);
  if (!upstream) {
    LOG_WARN() << "ReverseProxy: no upstream registered for '" << upstream_name_ << "'";
    SendError(client_conn, 502, "no upstream service");
    return;
  }

  runtime::upstream::Backend* backend = lb_->Select(*upstream);
  if (!backend) {
    LOG_WARN() << "ReverseProxy: no healthy backend for '" << upstream_name_ << "'";
    SendError(client_conn, 502, "no healthy backend");
    return;
  }

  backend->active_requests.fetch_add(1, std::memory_order_relaxed);

  std::string forwarded = req.SerializeToString();

  runtime::net::EventLoop* loop = client_conn->GetLoop();
  runtime::net::InetAddress backend_addr(backend->port, backend->host);

  auto tcp_client = std::make_shared<runtime::net::TcpClient>(
      loop, backend_addr, "proxy->" + backend->Address());
  tcp_client->SetConnectTimeout(connect_timeout_sec_);

  // Replace HttpContext with ProxyContext so OnMessage skips HTTP parsing on
  // this connection for the duration of the proxy session.
  ProxyContext ctx;
  ctx.backend    = backend;
  ctx.tcp_client = tcp_client;
  ctx.state      = ProxyContext::State::kConnecting;
  client_conn->SetContext(std::move(ctx));

  // Weak ref: callbacks must not extend the frontend connection's lifetime.
  auto weak_front = std::weak_ptr<runtime::net::TcpConnection>(client_conn);

  tcp_client->SetConnectCallback(
      [forwarded, weak_front](runtime::net::TcpConnection::TcpConnectionPtr backend_conn) {
        auto front = weak_front.lock();
        if (!front) {
          backend_conn->Shutdown();
          return;
        }
        auto* pctx = std::any_cast<ProxyContext>(&front->GetContext());
        if (pctx) {
          pctx->backend_conn = backend_conn;
          pctx->state        = ProxyContext::State::kForwarding;
        }
        backend_conn->Send(forwarded);
      });

  tcp_client->SetMessageCallback(
      [weak_front](runtime::net::TcpConnection::TcpConnectionPtr /*backend*/,
                   runtime::net::Buffer& buf,
                   runtime::time::Timestamp) {
        auto front = weak_front.lock();
        if (!front) return;
        front->Send(buf.RetrieveAllAsString());
      });

  tcp_client->SetCloseCallback(
      [weak_front, backend](
          runtime::net::TcpConnection::TcpConnectionPtr /*backend*/) {
        backend->active_requests.fetch_sub(1, std::memory_order_relaxed);
        auto front = weak_front.lock();
        if (front) front->Shutdown();
      });

  tcp_client->SetErrorCallback([weak_front, backend](int err) {
    backend->active_requests.fetch_sub(1, std::memory_order_relaxed);
    auto front = weak_front.lock();
    if (!front) return;
    const bool timed_out = (err == ETIMEDOUT);
    const int  status    = timed_out ? 504 : 502;
    const char* msg      = timed_out ? "gateway timeout" : "bad gateway";
    std::string body     = "{\"error\":\"" + std::string(msg) + "\"}";
    std::string resp =
        "HTTP/1.1 " + std::to_string(status) + " " + std::string(msg) +
        "\r\nContent-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) +
        "\r\nConnection: close\r\n\r\n" + body;
    front->Send(resp);
    front->Shutdown();
  });

  loop->RunInLoop([tcp_client] { tcp_client->Connect(); });
}

void ReverseProxy::SendError(const TcpConnectionPtr& conn,
                              int status,
                              std::string_view msg) {
  std::string body        = "{\"error\":\"" + std::string(msg) + "\"}";
  std::string status_text = (status == 502) ? "Bad Gateway" : "Service Unavailable";
  std::string resp =
      "HTTP/1.1 " + std::to_string(status) + " " + status_text +
      "\r\nContent-Type: application/json\r\n"
      "Content-Length: " + std::to_string(body.size()) +
      "\r\nConnection: close\r\n\r\n" + body;
  conn->Send(resp);
  conn->Shutdown();
}

}  // namespace runtime::proxy
