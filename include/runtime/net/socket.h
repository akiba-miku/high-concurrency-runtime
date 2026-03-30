#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/inet_address.h"

namespace runtime::net {

class Socket : public runtime::base::NonCopyable {
public:
    explicit Socket(int sockfd);
    ~Socket();
    
    int Fd() const { return sockfd_; }

    void BindAddress(const InetAddress &localaddr);
    void Listen();
    
    int Accept(InetAddress *peeraddr);

    void ShutdownWrite();

    void SetTcpNoDelay(bool on);
    void SetReuseAddr(bool on);
    void SetReusePort(bool on);
    void SetKeepAlive(bool on);
private:
    const int sockfd_;
};

}   // namespace runtime::net
