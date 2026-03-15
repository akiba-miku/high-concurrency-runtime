#include "runtime/net/net_utils.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <unistd.h>


namespace runtime::net {

namespace {
    bool setFdFlag(int fd, int cmd_get, int cmd_set, int new_flag)
    {
        int flags = ::fcntl(fd, cmd_get, 0);
        if(flags < 0)
        {
            return false;
        }

        if(::fcntl(fd, cmd_set, flags | new_flag) < 0) {
            return false;
        }
        return true;
    }

    bool SetReuseAddr(int fd, bool on)
    {
        int optval = on ? 1 : 0;

        // SOL_SOCKET: 表示在 Socket 层级进行设置
        // SO_REUSEADDR: 具体的选项名
        // &optval: 存入 1 表示开启
        int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t)(optval));
        return ret == 0;
    }

    bool SetReusePort(int fd, bool on)
    {
        int optval = on ? 1 : 0;
        int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, (socklen_t)sizeof(optval));
        return ret == 0;
    }

    bool TcpNonDelay(int fd, bool on)
    {
        int optval = on ? 1 : 0;
        int ret = setsockopt(fd, SOL_SOCKET, TCP_NODELAY, &optval, (socklen_t)sizeof(optval));
        return ret == 0;
    }

    bool SetKeepAlive(int fd, bool on)
    {
        int optval = on ? 1 : 0;
        int ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, (socklen_t)sizeof(optval));
        return ret == 0;
    }
}
}