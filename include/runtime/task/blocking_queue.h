#pragma once

#include "runtime/base/noncopyable.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

namespace runtime::task {

// Thread-safe bounded/unbounded blocking queue.
// capacity == 0 means unbounded.
template <typename T>
class BlockingQueue : public runtime::base::NonCopyable {
public:
    explicit BlockingQueue(std::size_t capacity = 0)
        : capacity_(capacity) {}

    void Push(T item) {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] {
            return closed_ || capacity_ == 0 || queue_.size() < capacity_;
        });
        if (closed_) return;
        queue_.push(std::move(item));
        not_empty_.notify_one();
    }

    bool TryPush(T item) {
        std::lock_guard lock(mutex_);
        if (closed_) return false;
        if (capacity_ > 0 && queue_.size() >= capacity_) return false;
        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> Pop() {
        std::unique_lock lock(mutex_);
        not_empty_.wait(lock, [&] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return item;
    }

    std::optional<T> TryPop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return item;
    }

    void Close() {
        std::lock_guard lock(mutex_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t Size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    bool Empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    bool Closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> queue_;
    std::size_t capacity_;
    bool closed_{false};
};

}  // namespace runtime::task
