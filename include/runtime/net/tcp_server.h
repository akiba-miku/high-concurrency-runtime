#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/acceptor.h"
#include "runtime/net/event_loop_thread_pool.h"
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
    using ThreadInitCallBack = EventLoopThreadPool::ThreadInitCallBack;
    
    TcpServer(EventLoop *loop, 
              const InetAddress &listenaddr, 
              const std::string &name);
    ~TcpServer();

    // 0 表示单线程，4 表示多线程
    void setThreadNum(int num_threads) {
        thread_num_ = num_threads;
    }

    void setThreadInitCallBack(ThreadInitCallBack &&cb) { 
        thread_init_callback_ = std::forward<ThreadInitCallBack>(cb);
    }
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

    EventLoop *loop_; // 主loop, 只做accept
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> thread_pool_;
    int thread_num_;
    bool started_;
    int next_conn_id_;


    ConnectionCallBack connection_callback_;
    MessageCallBack message_callback_;
    WriteCompleteCallBack write_complete_callback_;
    ThreadInitCallBack thread_init_callback_ ;

    ConnectionMap connections_;
};


}   // namespace runtime::net;