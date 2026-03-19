#pragma once

#include "poller.h"
#include <vector>
#include <sys/epoll.h>
#include <net/socket.h>
namespace runtime::net {

/**
 * epoll 的使用
 * epoll_create;
 * epoll_ctl; add/mod/del
 * epoll_wait;
 */
class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override ;

    // 重写基类的抽象方法
    runtime::time::Timestamp poll(int timesoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
private:
    static const int kInitEventListSize = 16;
    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const ;
    // 更新Channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event> ;
    
    int epollfd_;
    EventList events_;
};

}