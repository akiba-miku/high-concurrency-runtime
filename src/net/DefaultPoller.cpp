#include "include/runtime/net/poller.h"
#include <stdlib.h>
namespace runtime::net {

Poller *Poller::newDefaultPoller(EventLoop *loop) {
    if(::getenv("RUNTIME_USE_POLL")) {
        return nullptr; // 生成poll的实例
    }
    else {
        return nullptr; // 生成epoll的实例
    }
}
}