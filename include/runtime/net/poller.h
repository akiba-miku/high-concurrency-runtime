#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/time/timestamp.h"
#include <vector>
#include <unordered_map>
class Channel;
class EventLoop;
namespace runtime::net {
    
// 多路事件分发器
class Poller : public runtime::base::NonCopyable {
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop *loop);
    virtual ~Poller();

    // 给所有IO复用保留统一的接口
    virtual runtime::time::Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数Channel在当前Poller当中
    bool hasChannel(Channel *channel) const;

    // EventLoop通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    // key: fd -> value: Channel
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop* ownerLoop_;
};

}