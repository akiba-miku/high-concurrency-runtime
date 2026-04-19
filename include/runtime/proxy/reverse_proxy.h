#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/http/http_request.h"
#include "runtime/lb/load_balancer.h"
#include "runtime/net/tcp_connection.h"
#include "runtime/registry/service_registry.h"

#include <string>

namespace runtime::proxy {

// ReverseProxy implements transparent HTTP proxying for one named upstream.
//
// Handle() is called from HttpServer's IO thread after route matching. It:
//   1. Resolves the upstream and selects a healthy backend via the LB.
//   2. Creates a TcpClient, stores it in the frontend conn's ProxyContext so
//      its lifetime is tied to the connection.
//   3. On backend connect: serialises and forwards the original HTTP request.
//   4. On backend data: pipes response bytes back to the frontend unchanged.
//   5. On backend close: shuts down the frontend connection.
//   6. On error (no backend, connect failure, timeout): sends 502 or 504.
class ReverseProxy : public runtime::base::NonCopyable {
public:
  using TcpConnectionPtr = runtime::net::TcpConnection::TcpConnectionPtr;

  // registry and lb must outlive this object.
  ReverseProxy(runtime::registry::ServiceRegistry* registry,
               runtime::lb::LoadBalancer* lb,
               std::string upstream_name,
               double connect_timeout_sec = 5.0);

  // Called from the HttpServer IO thread. Does not block.
  void Handle(runtime::http::HttpRequest req, TcpConnectionPtr client_conn);

private:
  void SendError(const TcpConnectionPtr& conn, int status, std::string_view msg);

  runtime::registry::ServiceRegistry* registry_;
  runtime::lb::LoadBalancer*          lb_;
  std::string                         upstream_name_;
  double                              connect_timeout_sec_;
};

}  // namespace runtime::proxy
