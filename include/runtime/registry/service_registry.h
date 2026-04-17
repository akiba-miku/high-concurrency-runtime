#pragma once

#include "runtime/base/noncopyable.h"
#include "runtime/base/nonmoveable.h"
#include "runtime/upstream/upstream.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace runtime::registry {

// ServiceRegistry maps a logical service name (for example, "user_service")
// to its corresponding Upstream object
//
// Thread-safety:
// - Phase N1: not thread-safe for concurrent writes
// - Intented usage in Phase N1 is: populate the registry at startup, then
//   perform read-only lookups during request handing.
// - Later phases can add reader-writer locking or snapshot-bases updates for
//   dynamic registration and healthy-aware refresh
class ServiceRegistry : public runtime::base::NonCopyable,
                        public runtime::base::NonMoveable {
public:
  using UpstreamMap =
      std::unordered_map<std::string, runtime::upstream::Upstream>;

  ServiceRegistry() = default;

  // Redisters or replaces an upstream under the given service name
  // Returns false if name is empty.
  bool Register(std::string name, runtime::upstream::Upstream upstream);

  // Returns a pointer to the named upstream, or nullptr if not found
  runtime::upstream::Upstream *Get(std::string_view name) noexcept;
  const runtime::upstream::Upstream *Get(std::string_view name) const noexcept;

  // Read-only access to the whole table, typically for diagnostics or
  // startup verifycation
  const UpstreamMap &All() const noexcept { return table_; }
  
  bool Empty() const noexcept { return table_.empty(); }
  std::size_t Size() const noexcept { return table_.size(); }
private:
  UpstreamMap table_;
};
} // namespace runtime::registry