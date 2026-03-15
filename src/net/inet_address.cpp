#include "runtime/net/inet_address.h"

#include <arpa/inet.h>
#include <cstring>

namespace runtime::net {

InetAddress::InetAddress(runtime::base::u16 port, runtime::base::String ip) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);

    if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
        // 保持一个可用的默认地址，避免上层拿到未初始化的 sockaddr_in。
        addr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
}

runtime::base::String InetAddress::toIp() const {
    char buffer[INET_ADDRSTRLEN] = {0};
    const char* result = ::inet_ntop(AF_INET, &addr_.sin_addr, buffer, sizeof(buffer));
    return result == nullptr ? runtime::base::String() : runtime::base::String(result);
}

runtime::base::String InetAddress::toIpPort() const {
    return toIp() + ":" + std::to_string(toPort());
}

runtime::base::u16 InetAddress::toPort() const {
    return ntohs(addr_.sin_port);
}

}  // namespace runtime::net
