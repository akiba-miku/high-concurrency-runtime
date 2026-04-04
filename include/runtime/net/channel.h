#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/time/timestamp.h"

#include <functional>
#include <utility>
#include <memory>

namespace runtime::net {

class EventLoop ;
/**
 * Channel -> fd的事件代理对象: 封装fd和感兴趣的事件
 * Channel 是 事件解释器 + 回调分发器
 * 
 * fd + interested events + returned events + callbacks
 * 维护channel->poller中fd状态的一致性
 */
class Channel : public runtime::base::NonCopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(runtime::time::Timestamp)>;
    
    explicit Channel(EventLoop *loop, int fd); 
    ~Channel();

    // 核心：事件处理
    void HandleEvent(runtime::time::Timestamp receive_time);

    // 设置回调对象
    void SetReadCallback(ReadEventCallback&& cb) { read_callback_ = std::forward<ReadEventCallback>(cb); }
    void SetWriteCallback(EventCallback&& cb) { write_callback_ = std::forward<EventCallback>(cb); }
    void SetCloseCallback(EventCallback&& cb) { close_callback_ = std::forward<EventCallback>(cb); }
    void SetErrorCallback(EventCallback&& cb) { error_callback_ = std::forward<EventCallback>(cb); }

    // 保护回调宿主对象的生命周期
    void Tie(const std::shared_ptr<void>&);

    int Fd() const { return fd_; }
    int Events() const { return events_; }
    int Revents() const { return revents_; }
    void SetRevents(int revt) { revents_ = revt; }

    // 先修改本地状态机 然后同步底层。
    void EnableReading() { events_ |= kReadEvent; Update(); }
    void DisableReading() { events_ &= ~kReadEvent; Update(); }
    void EnableWriting() { events_ |= kWriteEvent; Update(); }
    void DisableWriting() { events_ &= ~kWriteEvent; Update(); }
    void DisableAll() { events_ = kNoneEvent; Update(); }

    // 返回fd当前的事件状态
    bool IsNoneEvent() const { return events_ == kNoneEvent; }
    bool IsWriting() const { return events_ & kWriteEvent; }
    bool IsReading() const { return events_ & kReadEvent; }

    int Index() const { return index_; }
    //Poller返回活跃事件->设置Channel的方法
    void SetIndex(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *OwnerLoop() { return loop_; }
    void Remove();
private:
    // 一致性：关心的事件变化同步给底层
    void Update();
    void HandleEventWithGuard(runtime::time::Timestamp receive_time);
private:
    //定义 "read/write/None" 常量
    static const int kNoneEvent; // -> 0
    static const int kReadEvent; // -> EPOLLIN | EPOLLPRI
    static const int kWriteEvent; // -> EPOLLOUT

    EventLoop *loop_; // 属于哪个eventloop
    const int fd_; // channel 绑定的哪个文件描述符
    int events_; // 想监听的事件
    int revents_; // 实际发生的事件
    int index_; // 底层poller的适配状态

    std::weak_ptr<void> tie_; // Channel 与宿主对象生命周期关联
    bool tied_;
    
    ReadEventCallback read_callback_; // 可读事件
    EventCallback write_callback_; // 可写事件
    EventCallback close_callback_; // 连接关闭 / 对端关闭
    EventCallback error_callback_; // 错误事件
};

}   // namespace runtime::net
