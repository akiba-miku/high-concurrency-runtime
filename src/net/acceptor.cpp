#include "runtime/net/acceptor.h"

#include "runtime/net/event_loop.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/net_utils.h"

#include <unistd.h>

namespace runtime::net {

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listen_addr, bool reuse_port)
    : loop_(loop),
      accept_socket_(CreateNonBlockingSocket()),
      accept_channel_(loop, accept_socket_.Fd()),
      listening_(false) {
    accept_socket_.SetReuseAddr(true);
    accept_socket_.SetReusePort(reuse_port);
    accept_socket_.BindAddress(listen_addr);

    accept_channel_.SetReadCallback(
        [this](runtime::time::Timestamp receive_time) {
            HandleRead(receive_time);
        });
}

Acceptor::~Acceptor() {
    accept_channel_.DisableAll();
    accept_channel_.Remove();
}
void Acceptor::Listen() {
    listening_ = true;
    accept_socket_.Listen();
    accept_channel_.EnableReading();
}

void Acceptor::HandleRead(runtime::time::Timestamp) {
    InetAddress peer_addr(0);
    int connfd = accept_socket_.Accept(&peer_addr);

    if (connfd >= 0) {
        if (new_connection_callback_) {
            new_connection_callback_(connfd, peer_addr);
        } else {
            ::close(connfd);
        }
    }
}

}  // namespace runtime::net
