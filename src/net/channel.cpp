#include "include/runtime/net/channel.h"
#include "include/runtime/log/logger.h"
#include "include/runtime/log/log_formatter.h"
#include <sys/epoll.h>

namespace runtime::net {

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , index_(-1)
    , tied_(false)
    {}

Channel::~Channel() {
    
}

// Channel 的tie方法什么时候调用过...
void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

/**
 * 改变Channel所表示的fd的Events事件后， update负责在Poller
 * 在更改fd相应的事件epoll_ctl
 */
void Channel::update() {
    // 通过Channel所属的eventloop, 调用poller的相应方法， 注册fd的events
    // loop_->updateChannel(this);
    // add code_;
}


void Channel::remove() {
    // add code_;
    // loop_->removeChannel();
}

void Channel::handleEvent(runtime::time::Timestamp receiveTime) {
    std::shared_ptr<void> guard;
    if(tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard) {
            handleEventWithGuard(receiveTime);
        }
    }
    else {
        handleEventWithGuard(receiveTime);
    }
}


void Channel::handleEventWithGuard(runtime::time::Timestamp receiveTime) {
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if(closeCallBack_) {
            closeCallBack_();
        }
    }

    if((revents_ & EPOLLERR)) {
        if(errorCallBack_) {
            errorCallBack_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI)) {
        if(readCallBack_) {
            readCallBack_(receiveTime);
        }
    }

    if(revents_ & (EPOLLOUT)) {
        if(writeCallBack_) {
            writeCallBack_();
        }
    }
}
}