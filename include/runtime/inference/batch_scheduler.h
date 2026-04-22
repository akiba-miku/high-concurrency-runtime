#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/inference/inference_request.h"
#include "runtime/inference/llama_engine.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

namespace runtime::inference {

// Manages a fixed pool of LlamaEngine instances for parallel inference.
// Requests are distributed round-robin across engines.
class BatchScheduler : public runtime::base::NonCopyable {
public:
    struct Config {
        LlamaEngine::Config engine;
        int max_concurrency{4};
    };

    explicit BatchScheduler(Config cfg);
    ~BatchScheduler();

    void Start();
    void Stop();

    void Submit(InferenceRequest req);

    std::size_t QueueDepth() const;

    bool IsReady() const {
        return ready_.load(std::memory_order_acquire);
    }

private:
    Config config_;
    std::vector<std::unique_ptr<LlamaEngine>> engines_;
    std::atomic<std::size_t> round_robin_{0};
    std::atomic<bool> ready_{false};
};

}  // namespace runtime::inference
