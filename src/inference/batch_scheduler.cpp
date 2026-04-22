#include "runtime/inference/batch_scheduler.h"

#include "runtime/log/logger.h"

#include <stdexcept>

namespace runtime::inference {

BatchScheduler::BatchScheduler(Config cfg)
    : config_(std::move(cfg)) {
    if (config_.max_concurrency <= 0) {
        throw std::invalid_argument("BatchScheduler: max_concurrency must be > 0");
    }
    engines_.reserve(static_cast<std::size_t>(config_.max_concurrency));
    for (int i = 0; i < config_.max_concurrency; ++i) {
        engines_.push_back(std::make_unique<LlamaEngine>(config_.engine));
    }
}

BatchScheduler::~BatchScheduler() {
    Stop();
}

void BatchScheduler::Start() {
    for (auto& engine : engines_) {
        engine->Start();
    }
    ready_.store(true, std::memory_order_release);
    LOG_INFO() << "BatchScheduler: started with "
               << config_.max_concurrency << " slot(s)";
}

void BatchScheduler::Stop() {
    if (!ready_.exchange(false)) return;
    for (auto& engine : engines_) {
        engine->Stop();
    }
    LOG_INFO() << "BatchScheduler: stopped";
}

void BatchScheduler::Submit(InferenceRequest req) {
    const std::size_t n   = engines_.size();
    const std::size_t idx = round_robin_.fetch_add(1, std::memory_order_relaxed) % n;
    engines_[idx]->Submit(std::move(req));
}

std::size_t BatchScheduler::QueueDepth() const {
    return 0;  // placeholder; wire up via metrics layer when needed
}

}  // namespace runtime::inference
