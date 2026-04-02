#include "runtime/net/tcp_server.h"
#include "runtime/log/logger.h"
#include "runtime/net/event_loop.h"
#include "runtime/net/net_utils.h"

#include <cstdio>

namespace runtime::net {

TcpServer::TcpServer(
    EventLoop *loop,
    const InetAddress &listenaddr,
    const std::string &name)
    : loop_(loop),
      name_(name),
      acceptor_(std::make_unique<Acceptor>(loop, listenaddr, true)),
      started_(false),
      next_conn_id_(1),
      thread_num_(0) {
    LOG_INFO() << "tcp server created: name=" << name_;

    acceptor_->SetNewConnectionCallback(
        [this](int sockfd, const InetAddress &peeraddr) {
            NewConnection(sockfd, peeraddr);
        }
    );
}

TcpServer::~TcpServer() {
    LOG_INFO() << "tcp server destroying: name=" << name_
               << " active_connections=" << connections_.size();
    for(auto &item : connections_) {
        TcpConnectionPtr conn(item.second);
        conn->ConnectDestroyed();
    }
}

// 1. 启动线程池
// 2. 开始监听
void TcpServer::Start() {
    if(!started_) {
        started_ = true;

        thread_pool_ = std::make_unique<EventLoopThreadPool>(loop_, thread_num_);
        thread_pool_->Start();
        LOG_INFO() << "tcp server starting: name=" << name_
                   << " io_threads=" << thread_num_;
        loop_->RunInLoop([this] {
            acceptor_->Listen();
        });
    }
}

// 最重要的函数
void TcpServer::NewConnection(int sockfd, const InetAddress &peeraddr) {
    // 1. 从线程池挑一个subLoop 负责后续IO
    EventLoop *ioLoop = thread_pool_->GetNextLoop();

    // 2. 生成连接名
    char buf[64];
    std::snprintf(buf, sizeof(buf), "#%d", next_conn_id_++);
    std::string conn_name = name_ + buf;

    // 3. 获取本端地址
    InetAddress localaddr(GetLocalAddr(sockfd));

    // 4. 创建 TcpConnection
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
        ioLoop,
        conn_name,
        sockfd,
        localaddr,
        peeraddr);

    connections_[conn_name] = conn;

    LOG_INFO() << "new tcp connection: name=" << conn_name
               << " local=" << localaddr.ToIpPort()
               << " peer=" << peeraddr.ToIpPort();

    conn->SetConnectionCallback(connection_callback_);
    conn->SetMessageCallback(message_callback_);
    conn->SetWriteCompleteCallback(write_complete_callback_);

    conn->SetCloseCallback([this](const TcpConnectionPtr &connection) {
        RemoveConnection(connection);
    });

    ioLoop->RunInLoop([conn] {
        conn->ConnectEstablished();
    });
}

// 无论连接在哪个ioLoop上关闭的， 都会回落到TcpServer的base_loop
void TcpServer::RemoveConnection(const TcpConnectionPtr &conn) {
    loop_->RunInLoop([this, conn] {
        RemoveConnectionInLoop(conn);
    });
}

void TcpServer::RemoveConnectionInLoop(const TcpConnectionPtr &conn) {
    LOG_INFO() << "removing tcp connection: name=" << conn->Name()
               << " peer=" << conn->PeerAddress().ToIpPort();
    connections_.erase(conn->Name());

    EventLoop *ioLoop = conn->GetLoop();
    ioLoop->QueueInLoop([conn] {
        conn->ConnectDestroyed();
    });
}

}   // namespace runtime::net
