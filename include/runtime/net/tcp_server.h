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

/**
 * TcpServer 封装了一个TCP服务器实例， 监听端口， 接受新连接；
 * 连接分配到一个新EventLoop， 创建持有TcpConnection.
 * 监听，接收， 分配， 创建，回收。
 */
class TcpServer : public runtime::base::NonCopyable {
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = TcpConnection::ConnectionCallback;
    using MessageCallback = TcpConnection::MessageCallback;
    using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;
    using ThreadInitCallback = EventLoopThreadPool::ThreadInitCallback;
    
    TcpServer(EventLoop *loop, 
              const InetAddress &listenaddr, 
              const std::string &name);
    ~TcpServer();

    // 0 表示单线程，>0 表示多线程
    void SetThreadNum(int num_threads) {
        thread_num_ = num_threads;
    }

    void SetThreadInitCallback(ThreadInitCallback&& cb) {
        thread_init_callback_ = std::forward<ThreadInitCallback>(cb);
    }
    void SetConnectionCallback(ConnectionCallback&& cb) {
        connection_callback_ = std::forward<ConnectionCallback>(cb);
    }

    void SetMessageCallback(MessageCallback&& cb) {
        message_callback_ = std::forward<MessageCallback>(cb);
    }

    void SetWriteCompleteCallback(WriteCompleteCallback&& cb) {
        write_complete_callback_ = std::forward<WriteCompleteCallback>(cb);
    }

    void Start();

private:
    void NewConnection(int sockfd, const InetAddress &peeraddr);
    void RemoveConnection(const TcpConnectionPtr &conn);
    void RemoveConnectionInLoop(const TcpConnectionPtr &conn);
private:
    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // 主loop, 只做accept
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> thread_pool_; // 多个subloop线程， 负责已建立连接的IO.
    int thread_num_;
    bool started_;
    int next_conn_id_;

    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    ThreadInitCallback thread_init_callback_;

    ConnectionMap connections_;
};


}   // namespace runtime::net;
