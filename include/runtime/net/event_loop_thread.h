#pragma once

#include "runtime/base/noncopyable.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace runtime::net {

class EventLoop;

class EventLoopThread : public runtime::base::NonCopyable {
public:
    using ThreadInitCallBack = std::function<void(EventLoop*)>;

    explicit EventLoopThread(
        const ThreadInitCallBack &cb = ThreadInitCallBack());
    
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    void threadFunc();
private:
    EventLoop *loop_;
    bool exiting_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallBack callback_;
};
} // namespace runtime::net