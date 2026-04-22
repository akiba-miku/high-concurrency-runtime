#pragma once

#include <atomic>
#include <memory>

namespace runtime::task {

class CancellationToken {
public:
    bool IsCancelled() const noexcept {
        return flag_ && flag_->load(std::memory_order_acquire);
    }

    static CancellationToken None() noexcept { return CancellationToken{}; }
    static CancellationToken Cancelled() noexcept {
        return CancellationToken{std::make_shared<std::atomic<bool>>(true)};
    }

private:
    friend class CancellationSource;
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> f)
        : flag_(std::move(f)) {}
    CancellationToken() = default;

    std::shared_ptr<std::atomic<bool>> flag_;
};

class CancellationSource {
public:
    CancellationSource()
        : flag_(std::make_shared<std::atomic<bool>>(false)) {}

    CancellationToken Token() const { return CancellationToken{flag_}; }
    void Cancel() noexcept { flag_->store(true, std::memory_order_release); }
    bool IsCancelled() const noexcept {
        return flag_->load(std::memory_order_acquire);
    }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

}  // namespace runtime::task
