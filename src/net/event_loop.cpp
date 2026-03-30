#include "runtime/net/event_loop.h"

#include "runtime/net/channel.h"
#include "runtime/net/poller.h"

#include <cassert>
#include <sys/eventfd.h>
#include <unistd.h>

namespace runtime::net {

namespace {

int CreateEventfd() {
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
      poller_(Poller::NewDefaultPoller(this)),
      wakeup_fd_(CreateEventfd()),
      wakeup_channel_(std::make_unique<Channel>(this, wakeup_fd_)) {
        assert(t_loop_in_this_thread == nullptr);
        t_loop_in_this_thread = this;

        wakeup_channel_->SetReadCallback([this](runtime::time::Timestamp) {
            HandleRead();
        });
        wakeup_channel_->EnableReading();
      }

EventLoop::~EventLoop() {
    wakeup_channel_->DisableAll();
    wakeup_channel_->Remove();
    ::close(wakeup_fd_);
    t_loop_in_this_thread = nullptr;
}

void EventLoop::Loop() {
    assert(!looping_);
    looping_ = true;
    quit_ = false;

    while(!quit_) {
        active_channels_.clear();
        poll_return_time_ = poller_->Poll(10000, &active_channels_);

        for(Channel *channel : active_channels_) {
            channel->HandleEvent(poll_return_time_);
        }

        DoPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::Quit() {
    quit_ = true;
    if(!IsInLoopThread()) {
        Wakeup();
    }
}

void EventLoop::RunInLoop(Functor cb){
    if(IsInLoopThread()){
        cb();
    } else {
        QueueInLoop(std::move(cb));
    }
}

void EventLoop::QueueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(cb));
    }

    if(!IsInLoopThread() || calling_pending_functors_) {
        Wakeup();
    }
}

void EventLoop::UpdateChannel(Channel *channel) {
    poller_->UpdateChannel(channel);
}

void EventLoop::RemoveChannel(Channel *channel) {
    poller_->RemoveChannel(channel);
}

bool EventLoop::HasChannel(Channel *channel){
    return poller_->HasChannel(channel);
}

bool EventLoop::IsInLoopThread() const {
    return thread_id_ == std::this_thread::get_id();
}

void EventLoop::Wakeup() {
    uint64_t one = 1;
    ::write(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::HandleRead() {
    uint64_t one = 1;
    ::read(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::DoPendingFunctors() {
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
