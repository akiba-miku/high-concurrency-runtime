#include "runtime/task/thread_pool.h"

namespace runtime::task {

ThreadPool::ThreadPool(std::size_t thread_count)
{
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
    }

    if (thread_count == 0) {
        thread_count = 4;
    }

    workers_.reserve(thread_count);

    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this](std::stop_token) {
            while (true) {
                Task task;

                {
                    std::unique_lock<std::mutex> lock(mutex_);

                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });

                    if (stop_ && tasks_.empty()) {
                        return;
                    }

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }

                task();
                --task_count_;
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    condition_.notify_all();

    // std::jthread 会在析构时自动 join，这里显式清空能更早完成回收。
    workers_.clear();
}

}  // namespace runtime::task
