#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/time/timestamp.h"

#include <functional>
#include <utility>
#include <memory>

namespace runtime::net {

class EventLoop ;
/**
 * Channel -> 封装fd 和 感兴趣的event
 * 绑定了poller返回的具体事件
 * 
 * fd + interested events + returned events + callbacks
 */
class Channel : public runtime::base::NonCopyable {
public:
    using EventCallBack = std::function<void()> ;
    using ReadEventCallBack = std::function<void(runtime::time::Timestamp)> ;
    
    explicit Channel(EventLoop *loop, int fd); 
    ~Channel();

    // fd 得到poller通知以后，处理事件的
    void handleEvent(runtime::time::Timestamp receiveTime);

    // 设置回调对象
    void setReadCallBack(ReadEventCallBack&& cb) {
        readCallBack_ = std::forward<ReadEventCallBack>(cb);
    }
    void setWriteCallBack(EventCallBack&& cb) {
        writeCallBack_ = std::forward<EventCallBack>(cb);
    }

    void setCloseCallBack(EventCallBack&& cb) {
        closeCallBack_ = std::forward<EventCallBack>(cb);
    }

    void setErrorCallBack(EventCallBack&& cb) {
        errorCallBack_ = std::forward<EventCallBack>(cb);
    }

    // 防止Channel被手动remove掉， channel依旧执行回调操作
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
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    void remove();
private:
    void update();
    void handleEventWithGuard(runtime::time::Timestamp receiveTime);
private:
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 属于哪个eventloop
    const int fd_; // channel 绑定的哪个文件符
    int events_; // 想监听的事件
    int revents_; // 实际发生的事件
    int index_;
    std::weak_ptr<void> tie_;
    bool tied_;
    
    ReadEventCallBack readCallBack_;
    EventCallBack writeCallBack_;
    EventCallBack closeCallBack_;
    EventCallBack errorCallBack_;
};

}   // namespace runtime::net