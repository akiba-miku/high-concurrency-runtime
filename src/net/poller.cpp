#include "runtime/net/poller.h"
#include "runtime/net/channel.h"
#include "runtime/net/epoll_poller.h"
namespace runtime::net {

Poller::Poller(EventLoop *loop) 
    : ownerLoop_(loop){}

bool Poller::hasChannel(Channel *channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

Poller *Poller::newDefaultPoller(EventLoop *loop) {
    return new EPollPoller(loop);
}

}   // namespace runtime::net

