#include "include/runtime/net/poller.h"
#include "include/runtime/net/channel.h"

namespace runtime::net {

Poller::Poller(EventLoop *loop) : ownerLoop_(loop){

}

bool Poller::hasChannel(Channel *channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

}