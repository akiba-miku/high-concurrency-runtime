#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/channel.h"
#include "runtime/net/socket.h"

#include <functional>

namespace runtime::net {

class EventLoop;
class InetAddress;

/**
 * Acceptor 监听端口+accept新连接
 */
class Acceptor : public runtime::base::NonCopyable {
public:
  using NewConnectionCallback =
      std::function<void(int sockfd, const InetAddress&)>;

  Acceptor(EventLoop *loop, const InetAddress &listen_addr, bool reuse_port);
  ~Acceptor();

  void SetNewConnectionCallback(const NewConnectionCallback &cb) {
    new_connection_callback_ = cb;
  }

  bool Listening() const { return listening_; }
  void Listen();

private:
  void HandleRead(runtime::time::Timestamp receive_time /*receive_time*/);

  EventLoop *loop_; // 主loop
  Socket accept_socket_; // 监听socket
  Channel accept_channel_; // 封装监听socket
  NewConnectionCallback new_connection_callback_; // 
  bool listening_; // 是否已经listen的状态位
};

} // namespace runtime::net
