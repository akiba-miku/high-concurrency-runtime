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

        using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
        using MessageCallback = std::function<void(
            const TcpConnectionPtr&,
            const std::string&,
            runtime::time::Timestamp)>;
        using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
        using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

        TcpConnection(
            EventLoop *loop,
            const std::string &name,
            int sockfd,
            const InetAddress &local_addr,
            const InetAddress &peeraddr);
        
        ~TcpConnection();

        EventLoop *GetLoop() const { return loop_; }
        const std::string &Name() const { return name_; }

        const InetAddress &LocalAddress() const { return local_addr_; }
        const InetAddress &PeerAddress() const { return peer_addr_; }

        bool Connected() const { return state_ == StateE::kConnected; }

        void Send(const std::string &message);
        void Shutdown();

        void SetConnectionCallback(const ConnectionCallback &cb) {
            connection_callback_ = cb;
        }

        void SetMessageCallback(const MessageCallback &cb) {
            message_callback_ = cb;
        }

        void SetCloseCallback(const CloseCallback &cb) {
            close_callback_ = cb;
        }

        void SetWriteCompleteCallback(const WriteCompleteCallback &cb) {
            write_complete_callback_ = cb;
        }

        void ConnectEstablished();
        void ConnectDestroyed();

private:
      enum class StateE {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
      };

      void SetState(StateE state) { state_ = state; }

      void HandleRead(runtime::time::Timestamp receive_time);
      void HandleWrite();
      void HandleClose();
      void HandleError();

      void SendInLoop(const std::string &message);
      void ShutdownInLoop();

private:
      EventLoop *loop_;
      const std::string name_;
      StateE state_;

      std::unique_ptr<Socket> socket_;
      std::unique_ptr<Channel> channel_;

      InetAddress local_addr_;
      InetAddress peer_addr_;

      std::string output_buffer_;

      ConnectionCallback connection_callback_;
      MessageCallback message_callback_;
      CloseCallback close_callback_;
      WriteCompleteCallback write_complete_callback_;
};

}   // namespace rumtime::net
