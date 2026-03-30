#include "runtime/net/socket.h"

#include <arpa/inet.h>
#include <cassert>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace runtime::net {

Socket::Socket(int sockfd)
    : sockfd_(sockfd) {}

Socket::~Socket() {
    ::close(sockfd_);
}

void Socket::BindAddress(const InetAddress &localAddr) {
    int ret = ::bind(sockfd_,
                    reinterpret_cast<const sockaddr*>(&localAddr.GetSockAddr()),
                    static_cast<socklen_t>(sizeof(sockaddr_in)));
    assert(ret == 0);
}

void Socket::Listen() {
    int ret = ::listen(sockfd_, SOMAXCONN);
    assert(ret == 0);
}

int Socket::Accept(InetAddress *peeraddr) {
    sockaddr_in addr{};
    socklen_t len = static_cast<socklen_t>(sizeof(addr));

#if defined(__linux__)
    int connfd = ::accept4(
        sockfd_,
        reinterpret_cast<sockaddr*>(&addr),
        &len,
        SOCK_NONBLOCK | SOCK_CLOEXEC);
#else 
    int connfd = ::accept(
        sockfd_,
        reinterpret_cast<sockaddr*>(&addr),
        &len);
#endif
    if(connfd >= 0 && peeraddr) {
        *peeraddr = InetAddress(addr);
    }                   

    return connfd;
}

void Socket::ShutdownWrite() {
    ::shutdown(sockfd_, SHUT_WR);
}

void Socket::SetTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}
void Socket::SetReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}
void Socket::SetReusePort(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::SetKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}
}   // namespace runtime::net
