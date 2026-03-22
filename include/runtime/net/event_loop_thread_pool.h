#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/net/event_loop_thread.h"

#include <memory>
#include <vector>

namespace runtime::net {

class EventLoop;

class EventLoopThreadPool : public runtime::base::NonCopyable {
public:
    using ThreadInitCallBack = EventLoopThread::ThreadInitCallBack;

    EventLoopThreadPool(EventLoop *base_loop, int num_threads);

    ~EventLoopThreadPool();

    void start(const ThreadInitCallBack &cb = ThreadInitCallBack());

    EventLoop *getNextLoop();
    std::vector<EventLoop*> getAllLoops() const;

    bool started() const { return started_; }
private:

    EventLoop *base_loop_;
    bool started_;
    int num_threads_;
    int next_;

    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};

}   // namespace runtime::net