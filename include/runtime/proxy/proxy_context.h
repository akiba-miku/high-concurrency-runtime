#pragma once

#include "runtime/net/tcp_client.h"
#include "runtime/net/tcp_connection.h"
#include "runtime/upstream/backend.h"

#include <memory>

namespace runtime::proxy {

// ProxyContext is stored in the frontend TcpConnection's context slot while a
// proxy session is active. It ties together the frontend connection, the
// backend TcpConnection, and the selected Backend metadata.
//
// Storing TcpClient here keeps it alive for the entire session: when the
// frontend connection is destroyed, its context (and therefore the TcpClient)
// is destroyed with it, closing the backend fd.
struct ProxyContext {
  enum class State { kConnecting, kForwarding, kClosed };

  std::shared_ptr<runtime::net::TcpClient> tcp_client;
  std::shared_ptr<runtime::net::TcpConnection> backend_conn;
  runtime::upstream::Backend* backend{nullptr};
  State state{State::kConnecting};
};

}  // namespace runtime::proxy
