#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/time/timestamp.h"

#include <unordered_map>
#include <vector>

namespace runtime::net {
    
class Channel;
class EventLoop;
// 多路事件分发器
class Poller : public runtime::base::NonCopyable {
public:
    using ChannelList = std::vector<Channel*>;

    explicit Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual runtime::time::Timestamp poll(int timeout_ms, ChannelList *active_channels) = 0;
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

}   // namespace runtime::net