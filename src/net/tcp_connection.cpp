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
        channel_->setReadCallBack(
            [this](runtime::time::Timestamp receive_time) {
                handleRead(receive_time);
            }
        );
        channel_->setWriteCallBack([this] {
            handleWrite();
        });
        channel_->setErrorCallBack([this] {
            handleError();
        });
        channel_->setCloseCallBack([this] {
            handleClose();
        });
      }

TcpConnection::~TcpConnection() = default;

void TcpConnection::send(const std::string &message) {
    if(state_ == StateE::kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(message);
        }
        else {
            auto self = shared_from_this();
            loop_->runInLoop([self, message] {
                self->sendInLoop(message);
            });
        }
    }
}

void TcpConnection::shutdown() {
    if(state_ == StateE::kConnected) {
        setState(StateE::kDisconnecting);
        auto self = shared_from_this();
        loop_->runInLoop([self] {
            self->shutdownInLoop();
        });
    }
}

void TcpConnection::connectEstablished() {
    setState(StateE::kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    if(connection_callback_) {
        connection_callback_(shared_from_this());
    }
}

void TcpConnection::connectDestroyed() {
    if(state_ == StateE::kConnected) {
        setState(StateE::kDisconnected);
        channel_->disableAll();
    }

    if(connection_callback_) {
        connection_callback_(shared_from_this());
    }

    channel_->remove();
}

void TcpConnection::handleRead(runtime::time::Timestamp receive_time) {
    char buf[4096];
    ssize_t n = ::read(channel_->fd(), buf, sizeof(buf));

    if(n > 0) {
        if(message_callback_) {
            message_callback_(
                shared_from_this(), 
                std::string(buf, static_cast<std::size_t>(n)),
                receive_time);
        }
    }
    else if(n == 0) {
        handleClose();
    }
    else {
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            handleError();
        }
    }
}

void TcpConnection::handleWrite() {
    if(channel_->isWriting()) {
        ssize_t n = ::write(
            channel_->fd(),
            output_buffer_.data(),
            output_buffer_.size()
        );

        if(n > 0) {
            output_buffer_.erase(0, static_cast<std::size_t>(n));

            if(output_buffer_.empty()) {
                channel_->disableWriting();
                if(write_complete_callback_) {
                    write_complete_callback_(shared_from_this());
                }

                if(state_ == StateE::kDisconnecting) {
                    shutdownInLoop();
                }
            }
        }
        else {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                handleError();
            }
        }
    }
}

void TcpConnection::handleClose() {
    setState(StateE::kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr guard(shared_from_this());
    if(connection_callback_) {
        connection_callback_(guard);
    }
    if(close_callback_) {
        close_callback_(guard);
    }
    
}

void TcpConnection::handleError() {
    int err = 0;
    socklen_t len = static_cast<socklen_t>(sizeof(err));
    ::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &err, &len);
}

void TcpConnection::sendInLoop(const std::string &message) {
    if(state_ == StateE::kDisconnected) {
        return ;
    }

    ssize_t nwrote = 0;
    if(!channel_->isWriting() && output_buffer_.empty()) {
        nwrote = ::write(channel_->fd(), message.data(), message.size());
        if(nwrote < 0) {
            nwrote = 0;
            if(errno != EWOULDBLOCK) {
                return ;
            }
        }
    }

    if(static_cast<std::size_t>(nwrote) < message.size()) {
        output_buffer_.append(message.data() + nwrote, message.size() - nwrote);
        if(!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
    else {
        if(write_complete_callback_) {
            write_complete_callback_(shared_from_this());
        }
    }
}

void TcpConnection::shutdownInLoop() {
    if(!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}
}
