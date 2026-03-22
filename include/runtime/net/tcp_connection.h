#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/channel.h"
#include "runtime/net/inet_address.h"
#include "runtime/net/socket.h"
#include "runtime/time/timestamp.h"

#include <functional>
#include <memory>
#include <string>

namespace runtime::net {

class EventLoop;

class TcpConnection
    : public runtime::base::NonCopyable,
      public std::enable_shared_from_this<TcpConnection> {
public:
        using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

        using ConnectionCallBack = std::function<void(const TcpConnectionPtr&)>;
        using MessageCallBack = std::function<void(
            const TcpConnectionPtr&,
            const std::string&,
            runtime::time::Timestamp)>;
        using CloseCallBack = std::function<void(const TcpConnectionPtr&)>;
        using WriteCompleteCallBack = std::function<void(const TcpConnectionPtr&)>;

        TcpConnection(
            EventLoop *loop,
            const std::string &name,
            int sockfd,
            const InetAddress &local_addr,
            const InetAddress &peeraddr);
        
        ~TcpConnection();

        EventLoop *getLoop() const { return loop_; }
        const std::string &name() const { return name_; }

        const InetAddress &localAddress() const { return local_addr_; }
        const InetAddress &peerAddress() const { return peer_addr_; }

        bool connected() const { return state_ == StateE::kConnected; }

        void send(const std::string &message);
        void shutdown();

        void setConnectionCallBack(const ConnectionCallBack &cb) {
            connection_callback_ = cb;
        }

        void setMessageCallBack(const MessageCallBack &cb) {
            message_callback_ = cb;
        }

        void setCloseCallBack(const CloseCallBack &cb) {
            close_callback_ = cb;
        }

        void setWriteCompleteCallBack(const WriteCompleteCallBack &cb) {
            write_complete_callback_ = cb;
        }

        void connectEstablished();
        void connectDestroyed();

private:
      enum class StateE {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
      };

      void setState(StateE state) { state_ = state; }

      void handleRead(runtime::time::Timestamp receive_time);
      void handleWrite();
      void handleClose();
      void handleError();

      void sendInLoop(const std::string &message);
      void shutdownInLoop();

private:
      EventLoop *loop_;
      const std::string name_;
      StateE state_;

      std::unique_ptr<Socket> socket_;
      std::unique_ptr<Channel> channel_;

      InetAddress local_addr_;
      InetAddress peer_addr_;

      std::string output_buffer_;

      ConnectionCallBack connection_callback_;
      MessageCallBack message_callback_;
      CloseCallBack close_callback_;
      WriteCompleteCallBack write_complete_callback_;
};

}   // namespace rumtime::net