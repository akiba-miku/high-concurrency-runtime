#include "runtime/net/epoll_poller.h"
#include "runtime/net/channel.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace runtime::net {

namespace {

constexpr int kNew = -1;     // channel未添加到poller，channel => index_
constexpr int kAdded = 1;    // 已添加到poller中
constexpr int kDeleted = 2;  // channel从poller删除

}  // namespace

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
    assert(epollfd_ >= 0);
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

runtime::time::Timestamp EPollPoller::poll(int timeout_ms,
                                           ChannelList* active_channels) {
    int num_events =
        ::epoll_wait(epollfd_, &*events_.begin(), events_.size(), timeout_ms);
    int saved_errno = errno;
    runtime::time::Timestamp now(runtime::time::Timestamp::now());

    if (num_events > 0) {
        fillActiveChannels(num_events, active_channels);
        if (static_cast<std::size_t>(num_events) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (num_events < 0 && saved_errno != EINTR) {
        errno = saved_errno;
    }

    return now;
}

void EPollPoller::fillActiveChannels(int num_events,
                                     ChannelList* active_channels) const {
    for (int i = 0; i < num_events; ++i) {
        auto* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        active_channels->push_back(channel);
    }
}

// channel => eventloop => Poller => epoll
void EPollPoller::updateChannel(Channel* channel) {
    const int index = channel->index();
    const int fd = channel->fd();

    // for debug
    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            channels_[fd] = channel;
        }

        // index == kDeleted
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {
        if (channel->isNonEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel) {
    const int fd = channel->fd();
    channels_.erase(fd);

    if (channel->index() == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel) {
    epoll_event event{};
    event.events = channel->events();
    event.data.ptr = channel;

    ::epoll_ctl(epollfd_, operation, channel->fd(), &event);
}

}  // namespace runtime::net