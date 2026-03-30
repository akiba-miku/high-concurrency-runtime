#include "runtime/net/channel.h"
#include "runtime/net/event_loop.h"

#include <sys/epoll.h>

namespace runtime::net {

/**
 *  EPOLLIN : 可读事件-> socket收到数据，对端发送FIN,accept新连接
 *  EPOLLPRI : 表示有紧急数据， TCP支持的一种OOB, 现实几乎不用，这里保留完整性
 *  EPOLLOUT : 可写事件-> socket发送缓冲区有空间可写数据
 */
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// 初始化事件代理状态
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , index_(-1)
    , tied_(false) {}

Channel::~Channel() = default;

// 避免use-after-free
// 使用weak_ptr 观察宿主对象的生命周期
void Channel::Tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

/**
 * 改变Channel所表示的fd的Events事件后， update负责在Poller
 * 在更改fd相应的事件epoll_ctl
 */
void Channel::Update() {
    // 通过Channel所属的eventloop, 调用poller的相应方法，注册fd的events
    loop_->UpdateChannel(this);
}

// 从EventLoop中删除Channel， 本质删除不再关心fd的事件代理。
void Channel::Remove() {
    loop_->RemoveChannel(this);
}

// handleEvent负责判断要不要生命周期保护
void Channel::HandleEvent(runtime::time::Timestamp receive_time) {
    if(tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard) {
            HandleEventWithGuard(receive_time);
        }
    }
    else {
        HandleEventWithGuard(receive_time);
    }
}

// 真正执行
void Channel::HandleEventWithGuard(runtime::time::Timestamp receive_time) {
    // 对端关闭 + 缓冲区数据判断
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if(close_callback_) {
            close_callback_();
        }
    }
    // EPOLLERR: 错误状态
    if((revents_ & EPOLLERR)) {
        if(error_callback_) {
            error_callback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI)) {
        if(read_callback_) {
            read_callback_(receive_time);
        }
    }

    if(revents_ & (EPOLLOUT)) {
        if(write_callback_) {
            write_callback_();
        }
    }
}
}
