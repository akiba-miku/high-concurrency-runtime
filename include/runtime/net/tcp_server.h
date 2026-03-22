#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/acceptor.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/tcp_connection.h"

#include <map>
#include <memory>
#include <string>

namespace runtime::net {

class EventLoop;
class TcpServer : public runtime::base::NonCopyable {
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallBack = TcpConnection::ConnectionCallBack;
    using MessageCallBack = TcpConnection::MessageCallBack;
    using WriteCompleteCallBack = TcpConnection::WriteCompleteCallBack;

    TcpServer(EventLoop *loop, const InetAddress &listenaddr, const std::string &name);
    ~TcpServer();

    void setConnectionCallBack(ConnectionCallBack &&cb) {
        connection_callback_ = std::forward<ConnectionCallBack>(cb);
    }

    void setMessageCallBack(MessageCallBack &&cb) {
        message_callback_ = std::forward<MessageCallBack>(cb);
    }

    void setWriteCompleteCallBack(WriteCompleteCallBack &&cb) {
        write_complete_callback_ = std::forward<WriteCompleteCallBack>(cb);
    }

    void start();

private:
    void newConnection(int sockfd, const InetAddress &peeraddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
private:
    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    bool started_;
    int next_conn_id_;

    ConnectionCallBack connection_callback_;
    MessageCallBack message_callback_;
    WriteCompleteCallBack write_complete_callback_;

    ConnectionMap connections_;
};


}   // namespace runtime::net;