#pragma once

#include "runtime/time/timestamp.h"

#include <optional>

namespace runtime::cache {

// Evict Reason
enum class EvictReason : uint8_t {
  kCapacity,
  kExpired,
  kExplicit,
  kReplaced,
};

template <typename Key, typename Value> class IEvictionPolicy {
public:
  virtual ~IEvictionPolicy() = default;

  virtual std::optional<Value> get(const Key &key) = 0;
  virtual void put(const Key &key, const Value &value,
                   std::optional<runtime::time::Timestamp> expire_at) = 0;
  virtual bool erase(const Key &key) = 0;
  virtual std::size_t size() const = 0;
  virtual void clear() = 0;
  virtual std::size_t prune_expired() = 0;
};
} // namespace runtime::cache