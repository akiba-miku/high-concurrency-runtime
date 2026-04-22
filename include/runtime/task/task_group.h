#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/task/scheduler.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>

namespace runtime::task {

// Waits for a batch of tasks submitted to a Scheduler to all complete.
// Rethrows the first exception on Wait().
class TaskGroup : public runtime::base::NonCopyable {
public:
    explicit TaskGroup(Scheduler& scheduler) : scheduler_(scheduler) {}

    void Run(std::function<void()> fn) {
        pending_.fetch_add(1, std::memory_order_relaxed);
        scheduler_.Submit([this, fn = std::move(fn)]() mutable {
            try {
                fn();
            } catch (...) {
                std::lock_guard lock(mutex_);
                if (!first_exception_) first_exception_ = std::current_exception();
            }
            if (pending_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                cv_.notify_all();
        });
    }

    void Wait() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] { return pending_.load(std::memory_order_acquire) == 0; });
        if (first_exception_) std::rethrow_exception(first_exception_);
    }

private:
    Scheduler& scheduler_;
    std::atomic<int> pending_{0};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::exception_ptr first_exception_;
};

}  // namespace runtime::task
