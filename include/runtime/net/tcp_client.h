#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/channel.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_connection.h"
#include "runtime/net/timer_id.h"

#include <functional>
#include <memory>
#include <string>

namespace runtime::net {

class EventLoop;

// TcpClient manages a single outbound non-blocking TCP connection.
//
// Connect() initiates a non-blocking connect() and registers the fd with the
// owning EventLoop. On success, ConnectCallback fires with the established
// TcpConnection; on failure or timeout, ErrorCallback fires with errno and
// the fd is closed.
//
// Ownership: the TcpClient must remain alive until one of the callbacks fires.
// Storing it in a shared_ptr inside the connection's context (SetContext) is
// a convenient way to tie its lifetime to the proxy session.
class TcpClient : public runtime::base::NonCopyable {
public:
  using TcpConnectionPtr   = TcpConnection::TcpConnectionPtr;
  using ConnectionCallback = std::function<void(TcpConnectionPtr)>;
  using ErrorCallback      = std::function<void(int /*errno*/)>;
  using MessageCallback    = TcpConnection::MessageCallback;
  using CloseCallback      = TcpConnection::CloseCallback;

  TcpClient(EventLoop* loop,
            const InetAddress& server_addr,
            const std::string& name);
  ~TcpClient();

  // Must be called from the owning loop thread.
  void Connect();

  void SetConnectCallback(ConnectionCallback&& cb) {
    connect_callback_ = std::forward<ConnectionCallback>(cb);
  }
  void SetErrorCallback(ErrorCallback&& cb) {
    error_callback_ = std::forward<ErrorCallback>(cb);
  }
  void SetMessageCallback(MessageCallback&& cb) {
    message_callback_ = std::forward<MessageCallback>(cb);
  }
  void SetCloseCallback(CloseCallback&& cb) {
    close_callback_ = std::forward<CloseCallback>(cb);
  }

  // Triggers ErrorCallback(ETIMEDOUT) if the connection is not established
  // within this many seconds. 0 disables the timeout. Default: 5.0.
  void SetConnectTimeout(double seconds) { timeout_sec_ = seconds; }

  const TcpConnectionPtr& Connection() const { return connection_; }

private:
  void HandleConnect();
  void OnConnectTimeout();
  void Fail(int err);

private:
  EventLoop*   loop_;
  InetAddress  server_addr_;
  std::string  name_;

  int sockfd_{-1};
  std::unique_ptr<Channel> channel_;
  TcpConnectionPtr connection_;

  double  timeout_sec_{5.0};
  TimerId timeout_timer_;

  ConnectionCallback connect_callback_;
  ErrorCallback      error_callback_;
  MessageCallback    message_callback_;
  CloseCallback      close_callback_;
};

}  // namespace runtime::net
