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

    acceptor_->setNewConnectionCallBack(
        [this](int sockfd, const InetAddress &peeraddr) {
            newConnection(sockfd, peeraddr);
        }
    );
}

TcpServer::~TcpServer() {
    for(auto &item : connections_) {
        TcpConnectionPtr conn(item.second);
        conn->connectDestroyed();
    }
}

void TcpServer::start() {
    if(!started_) {
        started_ = true;

        thread_pool_ = std::make_unique<EventLoopThreadPool>(loop_, thread_num_);
        thread_pool_->start();
        loop_->runInLoop([this]{
            acceptor_->listen();
        });
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peeraddr) {
    EventLoop *ioLoop = thread_pool_->getNextLoop();

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

    conn->setConnectionCallBack(connection_callback_);
    conn->setMessageCallBack(message_callback_);
    conn->setWriteCompleteCallBack(write_complete_callback_);

    conn->setCloseCallBack([this](const TcpConnectionPtr &connection){
        removeConnection(connection);
    });

    ioLoop->runInLoop([conn] {
        conn->connectEstablished();
    });
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    loop_->runInLoop([this, conn] {
        removeConnectionInLoop(conn);
    });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    connections_.erase(conn->name());

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop([conn] {
        conn->connectDestroyed();
    });
}

}   // namespace runtime::net