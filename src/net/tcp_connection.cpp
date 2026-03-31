#include "runtime/net/tcp_connection.h"

#include "runtime/net/event_loop.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace runtime::net {
TcpConnection::TcpConnection(
    EventLoop *loop,
    const std::string &name,
    int sockfd,
    const InetAddress &local_addr,
    const InetAddress &peer_addr)
    : loop_(loop),
      name_(name),
      state_(StateE::kConnecting),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      local_addr_(local_addr),
      peer_addr_(peer_addr) {
        channel_->SetReadCallback(
            [this](runtime::time::Timestamp receive_time) {
                HandleRead(receive_time);
            }
        );
        channel_->SetWriteCallback([this] {
            HandleWrite();
        });
        channel_->SetErrorCallback([this] {
            HandleError();
        });
        channel_->SetCloseCallback([this] {
            HandleClose();
        });
      }

TcpConnection::~TcpConnection() = default;

void TcpConnection::Send(const std::string &message) {
    if(state_ == StateE::kConnected) {
        if(loop_->IsInLoopThread()) {
            SendInLoop(message);
        }
        else {
            auto self = shared_from_this();
            loop_->RunInLoop([self, message] {
                self->SendInLoop(message);
            });
        }
    }
}

void TcpConnection::Shutdown() {
    if(state_ == StateE::kConnected) {
        SetState(StateE::kDisconnecting);
        auto self = shared_from_this();
        loop_->RunInLoop([self] {
            self->ShutdownInLoop();
        });
    }
}

void TcpConnection::ConnectEstablished() {
    SetState(StateE::kConnected);
    channel_->Tie(shared_from_this());
    channel_->EnableReading();

    if(connection_callback_) {
        connection_callback_(shared_from_this());
    }
}

void TcpConnection::ConnectDestroyed() {
    if(state_ == StateE::kConnected) {
        SetState(StateE::kDisconnected);
        channel_->DisableAll();
    }

    if(connection_callback_) {
        connection_callback_(shared_from_this());
    }

    channel_->Remove();
}

void TcpConnection::HandleRead(runtime::time::Timestamp receive_time) {
    int saved_errno = 0;
    ssize_t n = input_buffer_.ReadFd(channel_->Fd(), &saved_errno);

    if(n > 0) {
        if(message_callback_) {
            message_callback_(
                shared_from_this(), 
                input_buffer_.RetrieveAllAsString(),
                receive_time);
        }
    }
    else if(n == 0) {
        HandleClose();
    }
    else {
        errno = saved_errno;
        if(saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
            HandleError();
        }
    }
}

void TcpConnection::HandleWrite() {
    if(channel_->IsWriting()) {
        int saved_errno = 0;
        ssize_t n = output_buffer_.WriteFd(channel_->Fd(), &saved_errno);

        if(n > 0) {
            if(output_buffer_.ReadableBytes() == 0) {
                channel_->DisableWriting();
                if(write_complete_callback_) {
                    write_complete_callback_(shared_from_this());
                }

                if(state_ == StateE::kDisconnecting) {
                    ShutdownInLoop();
                }
            }
        }
        else {
            errno = saved_errno;
            if(saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
                HandleError();
            }
        }
    }
}

void TcpConnection::HandleClose() {
    SetState(StateE::kDisconnected);
    channel_->DisableAll();

    TcpConnectionPtr guard(shared_from_this());
    if(connection_callback_) {
        connection_callback_(guard);
    }
    if(close_callback_) {
        close_callback_(guard);
    }
    
}

void TcpConnection::HandleError() {
    int err = 0;
    socklen_t len = static_cast<socklen_t>(sizeof(err));
    ::getsockopt(channel_->Fd(), SOL_SOCKET, SO_ERROR, &err, &len);
}

void TcpConnection::SendInLoop(const std::string &message) {
    if(state_ == StateE::kDisconnected) {
        return ;
    }

    ssize_t nwrote = 0;
    if(!channel_->IsWriting() && output_buffer_.ReadableBytes() == 0) {
        nwrote = ::write(channel_->Fd(), message.data(), message.size());
        if(nwrote < 0) {
            nwrote = 0;
            if(errno != EWOULDBLOCK) {
                return ;
            }
        }
    }

    if(static_cast<std::size_t>(nwrote) < message.size()) {
        output_buffer_.Append(message.data() + nwrote, message.size() - nwrote);
        if(!channel_->IsWriting()) {
            channel_->EnableWriting();
        }
    }
    else {
        if(write_complete_callback_) {
            write_complete_callback_(shared_from_this());
        }
    }
}

void TcpConnection::ShutdownInLoop() {
    if(!channel_->IsWriting()) {
        socket_->ShutdownWrite();
    }
}
}
