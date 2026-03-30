#include "runtime/net/tcp_server.h"
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
      thread_pool_(std::make_unique<EventLoopThreadPool>(loop,0)),
      started_(false),
      next_conn_id_(1),
      thread_num_(0) {

    acceptor_->SetNewConnectionCallback(
        [this](int sockfd, const InetAddress &peeraddr) {
            NewConnection(sockfd, peeraddr);
        }
    );
}

TcpServer::~TcpServer() {
    for(auto &item : connections_) {
        TcpConnectionPtr conn(item.second);
        conn->ConnectDestroyed();
    }
}

void TcpServer::Start() {
    if(!started_) {
        started_ = true;

        thread_pool_ = std::make_unique<EventLoopThreadPool>(loop_, thread_num_);
        thread_pool_->Start();
        loop_->RunInLoop([this] {
            acceptor_->Listen();
        });
    }
}

void TcpServer::NewConnection(int sockfd, const InetAddress &peeraddr) {
    EventLoop *ioLoop = thread_pool_->GetNextLoop();

    char buf[64];
    std::snprintf(buf, sizeof(buf), "#%d", next_conn_id_);
    ++next_conn_id_;

    std::string conn_name = name_ + buf;

    InetAddress localaddr(GetLocalAddr(sockfd));

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
        ioLoop,
        conn_name,
        sockfd,
        localaddr,
        peeraddr);

    connections_[conn_name] = conn;

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

void TcpServer::RemoveConnection(const TcpConnectionPtr &conn) {
    loop_->RunInLoop([this, conn] {
        RemoveConnectionInLoop(conn);
    });
}

void TcpServer::RemoveConnectionInLoop(const TcpConnectionPtr &conn) {
    connections_.erase(conn->Name());

    EventLoop *ioLoop = conn->GetLoop();
    ioLoop->QueueInLoop([conn] {
        conn->ConnectDestroyed();
    });
}

}   // namespace runtime::net
