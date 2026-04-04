#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/inet_address.h"

namespace runtime::net {

/**
 * fd 的RAII包装， socket系统调用的薄封装
 * 拥有完整的生命周期， 和提供底层操作的接口
 */
class Socket : public runtime::base::NonCopyable {
public:
  explicit Socket(int sockfd);
  ~Socket();

  int Fd() const { return sockfd_; }

  // bind : 通常用于listen_fd
  void BindAddress(const InetAddress &localaddr);
  // listen : 通常用于listen_fd
  void Listen();
  // accept
  int Accept(InetAddress *peeraddr);
  // shutdown : 关闭写端， 半关闭
  void ShutdownWrite();

  // setsocketopt
  void SetTcpNoDelay(bool on); // true: 关闭Nagle算法, 减少小包发送延迟； false; 合并小包提高带宽利用率
  void SetReuseAddr(bool on); // 允许复用ip地址
  void SetReusePort(bool on); // 允许复用端口
  void SetKeepAlive(bool on); // 允许tcp keeplive 机制

private:
  const int sockfd_;
};

} // namespace runtime::net
