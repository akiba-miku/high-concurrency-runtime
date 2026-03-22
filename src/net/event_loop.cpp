#include "runtime/net/event_loop.h"

#include "runtime/net/channel.h"
#include "runtime/net/poller.h"

#include <cassert>
#include <sys/eventfd.h>
#include <unistd.h>

namespace runtime::net {

namespace {

int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    assert(evtfd >= 0);
    return evtfd;
}
thread_local EventLoop *t_loop_in_this_thread = nullptr;
}   // namespace

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      calling_pending_functors_(false),
      thread_id_(std::this_thread::get_id()),
      poller_(Poller::newDefaultPoller(this)),
      wakeup_fd_(createEventfd()),
      wakeup_channel_(std::make_unique<Channel>(this, wakeup_fd_)) {
        assert(t_loop_in_this_thread == nullptr);
        t_loop_in_this_thread = this;

        wakeup_channel_->setReadCallBack([this](runtime::time::Timestamp){
            handleRead();
        });
        wakeup_channel_->enableReading();
      }

EventLoop::~EventLoop() {
    wakeup_channel_->disableAll();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
    t_loop_in_this_thread = nullptr;
}

void EventLoop::loop() {
    assert(!looping_);
    looping_ = true;
    quit_ = false;

    while(!quit_) {
        active_channels_.clear();
        poll_return_time_ = poller_->poll(10000, &active_channels_);

        for(Channel *channel : active_channels_) {
            channel->handleEvent(poll_return_time_);
        }

        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if(!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(cb));
    }

    if(!isInLoopThread() || calling_pending_functors_) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel *channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}

bool EventLoop::isInLoopThread() const {
    return thread_id_ == std::this_thread::get_id();
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ::write(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::handleRead(){
    uint64_t one = 1;
    ::read(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    calling_pending_functors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }

    for(auto &functor : functors) {
        functor();
    }

    calling_pending_functors_ = false;
}
} // namespace runtime::net
