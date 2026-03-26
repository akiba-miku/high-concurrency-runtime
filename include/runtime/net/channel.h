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
 */
class Channel : public runtime::base::NonCopyable {
public:
    using EventCallBack = std::function<void()> ;
    using ReadEventCallBack = std::function<void(runtime::time::Timestamp)> ;
    
    explicit Channel(EventLoop *loop, int fd); 
    ~Channel();

    void handleEvent(runtime::time::Timestamp receiveTime);

    // 设置回调对象
    void setReadCallBack(ReadEventCallBack&& cb) { readCallBack_ = std::forward<ReadEventCallBack>(cb); }
    void setWriteCallBack(EventCallBack&& cb) { writeCallBack_ = std::forward<EventCallBack>(cb); }
    void setCloseCallBack(EventCallBack&& cb) { closeCallBack_ = std::forward<EventCallBack>(cb); }
    void setErrorCallBack(EventCallBack&& cb) { errorCallBack_ = std::forward<EventCallBack>(cb); }

    // 保护回调宿主对象的生命周期
    void tie(const std::shared_ptr<void>&) ;

    int fd() const { return fd_; }
    int events() const { return events_; }
    int revents() const { return revents_; }
    void set_revents(int revt) { revents_ = revt; }

    void enableReading() { events_ |= kReadEvent; update();}
    void disableReading() { events_ &= ~kReadEvent; update();}
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNonEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() {return index_;}
    //Poller返回活跃事件->设置Channel的方法
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    void remove();
private:
    // 关心的事件变化同步给底层
    void update();
    void handleEventWithGuard(runtime::time::Timestamp receiveTime);
private:
    //定义 "read/write/None" 常量
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 属于哪个eventloop
    const int fd_; // channel 绑定的哪个文件符
    int events_; // 想监听的事件
    int revents_; // 实际发生的事件
    int index_;

    std::weak_ptr<void> tie_; // Channel 与宿主对象生命周期关联
    bool tied_;
    
    ReadEventCallBack readCallBack_;
    EventCallBack writeCallBack_;
    EventCallBack closeCallBack_;
    EventCallBack errorCallBack_;
};

}   // namespace runtime::net