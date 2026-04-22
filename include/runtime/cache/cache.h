#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/cache/eviction_policy.h"
#include "runtime/cache/cache_options.h"
#include "runtime/time/timestamp.h"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace runtime::cache {

// Thread-safe cache that delegates eviction strategy to IEvictionPolicy<K,V>.
//
// Usage:
//   auto policy = std::make_unique<LruPolicy<K,V>>(capacity, segments);
//   Cache<K,V> cache(std::move(policy), options);
//   cache.Put(key, value);
//   auto v = cache.Get(key);  // returns std::optional<V>
template <typename Key, typename Value>
class Cache : public runtime::base::NonCopyable {
public:
    using Loader = std::function<std::optional<Value>(const Key&)>;

    explicit Cache(std::unique_ptr<IEvictionPolicy<Key, Value>> policy,
                   CacheOptions opts = {})
        : policy_(std::move(policy)), opts_(opts) {}

    // Returns the cached value, invoking loader on miss if provided.
    std::optional<Value> Get(const Key& key,
                             Loader loader = nullptr) {
        {
            std::lock_guard lock(mutex_);
            auto v = policy_->get(key);
            if (v) return v;
        }
        if (!loader) return std::nullopt;

        auto loaded = loader(key);
        if (loaded) {
            std::lock_guard lock(mutex_);
            policy_->put(key, *loaded, ComputeExpiry());
        }
        return loaded;
    }

    void Put(const Key& key, const Value& value,
             std::optional<runtime::time::Timestamp> expire_at = std::nullopt) {
        std::lock_guard lock(mutex_);
        policy_->put(key, value, expire_at ? expire_at : ComputeExpiry());
    }

    bool Erase(const Key& key) {
        std::lock_guard lock(mutex_);
        return policy_->erase(key);
    }

    void Clear() {
        std::lock_guard lock(mutex_);
        policy_->clear();
    }

    std::size_t Size() const {
        std::lock_guard lock(mutex_);
        return policy_->size();
    }

    std::size_t PruneExpired() {
        std::lock_guard lock(mutex_);
        return policy_->prune_expired();
    }

private:
    std::optional<runtime::time::Timestamp> ComputeExpiry() const {
        if (opts_.default_ttl_sec <= 0.0) return std::nullopt;
        return runtime::time::AddTime(
            runtime::time::Timestamp::Now(), opts_.default_ttl_sec);
    }

    mutable std::mutex mutex_;
    std::unique_ptr<IEvictionPolicy<Key, Value>> policy_;
    CacheOptions opts_;
};

}  // namespace runtime::cache
