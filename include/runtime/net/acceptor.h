#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/channel.h"
#include "runtime/net/socket.h"

#include <functional>

namespace runtime::net {

class EventLoop;
class InetAddress;

/**
 * acceptor 类 监听端口+accept新连接
 */
class Acceptor : public runtime::base::NonCopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress)>;

    Acceptor(EventLoop *loop, const InetAddress &listen_addr, bool reuse_port);
    ~Acceptor();

    void SetNewConnectionCallback(const NewConnectionCallback &cb) {
        new_connection_callback_ = cb;
    }

    bool Listening() const { return listening_; }
    void Listen();
private:
    void HandleRead(runtime::time::Timestamp receive_time);
    
    EventLoop *loop_;
    Socket accept_socket_;
    Channel accept_channel_;
    NewConnectionCallback new_connection_callback_;
    bool listening_;
};

}   // namespace runtime::net
