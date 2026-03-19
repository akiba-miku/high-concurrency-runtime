#include "runtime/net/epoll_poller.h"
#include "runtime/log/log_formatter.h"
#include "runtime/log/logger.h"
#include "runtime/net/channel.h"
#include <cerrno>
#include <cstring>

namespace runtime::net {

constexpr int kNew = -1; // channel未添加到poller， channel=>index_
constexpr int kAdded = 1; // 已添加到poller中
constexpr int kDeleted = 2; // channel从poller删除
namespace {
    EPollPoller::EPollPoller(EventLoop *loop) 
    : Poller(loop),
      epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize){
        if(epollfd_ < 0) {
          LOG_FATAL("EPollPoller: epoll create failed : %d\n", errno);
          exit(1);
        }
    }

    EPollPoller::~EPollPoller() {
      ::close(epollfd_);
    }

    runtime::time::Timestamp poll(int timesoutMs, ChannelList *activeChannels) {

    }

    // channel=> eventloop => Poller => epoll
    void EPollPoller::updateChannel(Channel *channel) {
      const int index = channel->index();
      // for debug
      LOG_INFO("fd=%d events=%d index=%d\n", channel->fd(), channel->events(), index);
      if(index == kNew || index == kDeleted) {
        int fd = channel->fd();
        if(index == kNew) {
          channels_[fd] = channel;
        }
        else {
        // index_ == kDeleted
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
      }
      else {
        int fd = channel->fd();
        if(channel->isNoneEvent()) {
          update(EPOLL_CTL_DEL, channel);
          channel->set_index(kDeleted);
        }
        else {
          update(EPOLL_CTL_MOD, channel);
        }
      }
    }

    void EPollPoller::removeChannel(Channel *channel) {
      int fd = channel->fd();
      int index = channel->index();
      if(index == kNew || index == kDeleted) {
        channels_.erase(fd);
      }

      if(index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
      }
      channel->set_index(kNew);
    }

    void EPollPoller::update(int operation, Channel *channel) {
      epoll_event event;
      memset(&event, 0, sizeof event);
      event.events = channel->events();
      event.data.ptr = channel;

      int fd = channel->fd();
      if(::epoll_ctl(epollfd_, operation, fd, &event) < 0 ) {
        if(operation == EPOLL_CTL_DEL) {
          LOG_ERROR("epoll_ctl del error: %d\n", errno);
        }
        else {
          LOG_FATAL("epoll_ctl add/mod bug: %d\n", errno);
          exit(1);
        }
      }
    }
}

}