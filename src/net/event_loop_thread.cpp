#include "runtime/net/event_loop_thread.h"

#include "runtime/net/event_loop.h"

#include <cassert>

namespace runtime::net {

EventLoopThread::EventLoopThread(const ThreadInitCallBack &cb)
    : loop_(nullptr),
      exiting_(false),
      callback_(cb) {}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if(loop_ != nullptr) {
        loop_->quit();
        if(thread_.joinable()) {
            thread_.join();
        }
    }
}

EventLoop *EventLoopThread::startLoop() {
    thread_ = std::thread(&EventLoopThread::threadFunc, this);

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr) {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;

    if(callback_) {
        callback_(&loop);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }
}
}   // namespace runtime::net