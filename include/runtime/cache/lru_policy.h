#pragma once

#include "eviction_policy.h"
#include "runtime/memory/segment_lru_cache.h"
#include "runtime/time/timestamp.h"

#include <cstddef>
#include <optional>

namespace runtime::cache {

template <typename Key, typename Value>
class LruPolicy : public IEvictionPolicy<Key, Value> {
public:
  explicit LruPolicy(std::size_t capacity, std::size_t segment_count)
      : cache_(capacity, segment_count) {}

  std::optional<Value> get(const Key &key) override { return cache_.get(key); }

  void put(const Key &key, const Value &value,
           std::optional<runtime::time::Timestamp> expire_at) override {
    if (expire_at.has_value()) {
      cache_.put(key, value, *expire_at);
    } else {
      cache_.put(key, value);
    }
  }

  bool erase(const Key &key) override { return cache_.erase(key); }

  std::size_t size() const override { return cache_.size(); }

  void clear() override { return cache_.clear(); }

  std::size_t prune_expired() override { return cache_.pruneExpired(); }

private:
  runtime::memory::SegmentLRUCache<Key, Value> cache_;
};
} // namespace runtime::cache