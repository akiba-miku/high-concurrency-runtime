#pragma once

#include "runtime/base/types.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace runtime::net {

// 对 sockaddr_in 做一层轻量封装，方便在上层网络模块中传递和格式化地址。
class InetAddress {
public:
    explicit InetAddress(
        runtime::base::u16 port,
        runtime::base::String ip = "127.0.0.1");
    explicit InetAddress(const struct sockaddr_in& addr) : addr_(addr) {}

    runtime::base::String toIp() const;
    runtime::base::String toIpPort() const;
    runtime::base::u16 toPort() const;

    const struct sockaddr_in& getSockaddr() const { return addr_; }

private:
    struct sockaddr_in addr_{};
};

}  // namespace runtime::net
