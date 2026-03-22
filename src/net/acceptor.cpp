#include "runtime/net/acceptor.h"

#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/net_utils.h"

#include <unistd.h>

namespace runtime::net {

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listen_addr, bool reuse_port)
    : loop_(loop),
      accept_socket_(CreateNonBlockingSocket()),
      accept_channel_(loop, accept_socket_.fd()),
      listening_(false) {
    accept_socket_.setReuseAddr(true);
    accept_socket_.setReusePort(reuse_port);
    accept_socket_.bindAddress(listen_addr);

    accept_channel_.setReadCallBack(
        [this](runtime::time::Timestamp receive_time) {
            handleRead(receive_time);
        });
}

Acceptor::~Acceptor() {
    accept_channel_.disableAll();
    accept_channel_.remove();
}
void Acceptor::listen() {
    listening_ = true;
    accept_socket_.listen();
    accept_channel_.enableReading();
}

void Acceptor::handleRead(runtime::time::Timestamp) {
    InetAddress peer_addr(0);
    int connfd = accept_socket_.accept(&peer_addr);

    if (connfd >= 0) {
        if (new_connection_callback_) {
            new_connection_callback_(connfd, peer_addr);
        } else {
            ::close(connfd);
        }
    }
}

}  // namespace runtime::net
