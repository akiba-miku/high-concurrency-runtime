#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

namespace runtime::net {

// 对 sockaddr_in 做一层轻量封装，方便在上层网络模块中传递和格式化地址。
class InetAddress {
public:
    explicit InetAddress(
        std::uint16_t port,
        std::string ip = "127.0.0.1");
    explicit InetAddress(const struct sockaddr_in& addr) : addr_(addr) {}

    std::string ToIp() const;
    std::string ToIpPort() const;
    std::uint16_t ToPort() const;

    const struct sockaddr_in& GetSockAddr() const { return addr_; }

private:
    struct sockaddr_in addr_{};
};

}  // namespace runtime::net
