#pragma once

#include "poller.h"

#include <sys/epoll.h>
#include <vector>

namespace runtime::net {
/**
 * epoll 的使用
 * epoll_create;
 * epoll_ctl; add/mod/del
 * epoll_wait;
 */
class EPollPoller : public Poller {
public:
    explicit EPollPoller(EventLoop *loop);
    ~EPollPoller() override ;

    // 重写基类的抽象方法
    runtime::time::Timestamp poll(int timesout_ms, ChannelList *active_channels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
private:
    static constexpr int kInitEventListSize = 16;
    // 填写活跃的连接
    void fillActiveChannels(int num_events, ChannelList *active_channels) const ;
    // 更新Channel通道
    void update(int operation, Channel *channel);
    
    int epollfd_;
    std::vector<epoll_event> events_;
};

}   // namespace runtime::net

