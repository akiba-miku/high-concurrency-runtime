#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/channel.h"
#include "runtime/net/socket.h"

#include <functional>

namespace runtime::net {

class EventLoop;
class InetAddress;

/**
 * acceptor 类 监听端口 + accept新连接
 */
class Acceptor : public runtime::base::NonCopyable {
public:
    using NewConnectionCallBack = std::function<void(int sockfd, const InetAddress)>;

    Acceptor(EventLoop *loop, const InetAddress &listen_addr, bool reuse_port);
    ~Acceptor();

    void setNewConnectionCallBack(const NewConnectionCallBack &cb){
        new_connection_callback_ = cb;
    }

    bool listening() const { return listening_; }
    void listen();
private:
    void handleRead(runtime::time::Timestamp receive_time);
    
    EventLoop *loop_;
    Socket accept_socket_;
    Channel accept_channel_;
    NewConnectionCallBack new_connection_callback_;
    bool listening_;
};

}   // namespace runtime::net
