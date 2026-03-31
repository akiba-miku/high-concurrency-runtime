#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/time/timestamp.h"

#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
namespace runtime::net {

class Channel;
class Poller;

// 时间循环类 主要包含了两个大模块 Channel Poller(epoll抽象)
// IO事件分发器 ， 线程内串行任务队列执行器
class EventLoop : public runtime::base::NonCopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void Loop();
    void Quit();

    runtime::time::Timestamp PollReturnTime() const { return poll_return_time_; }

    void RunInLoop(Functor cb);
    void QueueInLoop(Functor cb);

    void UpdateChannel(Channel *channel);
    void RemoveChannel(Channel *channel);
    bool HasChannel(Channel *channel) const;

    bool IsInLoopThread() const;

private:
    void Wakeup();
    void HandleRead();
    void DoPendingFunctors();

    std::atomic<bool> looping_; // 线程安全状态标志
    std::atomic<bool> quit_; // 退出标志
    std::atomic<bool> calling_pending_functors_; // 当前loop是否有需要执行的回调操作

    const std::thread::id thread_id_; // 存储当前事件循环所在线程的id
    runtime::time::Timestamp poll_return_time_; // poller返回发生事件的channels的时间点

    std::unique_ptr<Poller> poller_;
    std::vector<Channel*> active_channels_;

    // wakeup mechanism
    int wakeup_fd_; // main_loop 获取新用户的channel， 通过轮询算法选择一个subloop, 
                    //通过该成员通知唤醒subloop处理Channel
    std::unique_ptr<Channel> wakeup_channel_;

    std::mutex mutex_;
    std::vector<Functor> pending_functors_;
};

};
