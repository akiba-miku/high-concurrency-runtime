#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/inet_address.h"

namespace runtime::net {

class Socket : public runtime::base::NonCopyable {
public:
    explicit Socket(int sockfd);
    ~Socket();
    
    int fd() const { return sockfd_; }

    void bindAddress(const InetAddress &localaddr);
    void listen();
    
    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
private:
    const int sockfd_;
};

}   // namespace runtime::net

