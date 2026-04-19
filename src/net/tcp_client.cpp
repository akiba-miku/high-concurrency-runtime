#include "runtime/net/tcp_client.h"

#include "runtime/log/logger.h"
#include "runtime/net/event_loop.h"
#include "runtime/net/net_utils.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace runtime::net {

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& server_addr,
                     const std::string& name)
    : loop_(loop), server_addr_(server_addr), name_(name) {}

TcpClient::~TcpClient() {
  if (sockfd_ != -1) {
    if (channel_) {
      channel_->DisableAll();
      channel_->Remove();
      channel_.reset();
    }
    ::close(sockfd_);
  }
}

void TcpClient::Connect() {
  assert(loop_->IsInLoopThread());

  sockfd_ = CreateNonBlockingSocket();
  if (sockfd_ < 0) {
    LOG_ERROR() << "TcpClient: CreateNonBlockingSocket failed: " << std::strerror(errno);
    Fail(errno);
    return;
  }

  const struct sockaddr_in& sock_addr = server_addr_.GetSockAddr();
  int ret = ::connect(sockfd_,
                      reinterpret_cast<const struct sockaddr*>(&sock_addr),
                      static_cast<socklen_t>(sizeof(sock_addr)));
  int saved_errno = (ret == 0) ? 0 : errno;

  if (saved_errno != 0 && saved_errno != EINPROGRESS) {
    LOG_ERROR() << "TcpClient: connect() to " << server_addr_.ToIpPort()
                << " failed: " << std::strerror(saved_errno);
    ::close(sockfd_);
    sockfd_ = -1;
    Fail(saved_errno);
    return;
  }

  channel_ = std::make_unique<Channel>(loop_, sockfd_);
  channel_->SetWriteCallback([this] { HandleConnect(); });
  channel_->SetErrorCallback([this] {
    int err = 0;
    socklen_t len = sizeof(err);
    ::getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &err, &len);
    LOG_WARN() << "TcpClient: connect error to " << server_addr_.ToIpPort()
               << ": " << std::strerror(err);
    Fail(err != 0 ? err : ECONNREFUSED);
  });
  channel_->EnableWriting();

  if (timeout_sec_ > 0.0) {
    timeout_timer_ = loop_->RunAfter(timeout_sec_, [this] { OnConnectTimeout(); });
  }
}

void TcpClient::HandleConnect() {
  channel_->DisableAll();
  channel_->Remove();
  loop_->Cancel(timeout_timer_);

  int err = 0;
  socklen_t len = sizeof(err);
  ::getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &err, &len);

  if (err != 0) {
    LOG_WARN() << "TcpClient: connect to " << server_addr_.ToIpPort()
               << " failed: " << std::strerror(err);
    Fail(err);
    return;
  }

  int fd  = sockfd_;
  sockfd_ = -1;
  channel_.reset();

  InetAddress local_addr(GetLocalAddr(fd));
  connection_ = std::make_shared<TcpConnection>(loop_, name_, fd,
                                                local_addr, server_addr_);
  if (message_callback_) connection_->SetMessageCallback(message_callback_);
  if (close_callback_)   connection_->SetCloseCallback(close_callback_);
  connection_->ConnectEstablished();

  if (connect_callback_) {
    connect_callback_(connection_);
  }
}

void TcpClient::OnConnectTimeout() {
  LOG_WARN() << "TcpClient: connect timeout (" << timeout_sec_
             << "s) to " << server_addr_.ToIpPort();
  Fail(ETIMEDOUT);
}

void TcpClient::Fail(int err) {
  if (sockfd_ != -1) {
    if (channel_) {
      channel_->DisableAll();
      channel_->Remove();
      channel_.reset();
    }
    ::close(sockfd_);
    sockfd_ = -1;
  }
  if (error_callback_) {
    error_callback_(err);
  }
}

}  // namespace runtime::net
