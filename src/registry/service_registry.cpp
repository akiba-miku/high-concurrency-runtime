#include "runtime/registry/service_registry.h"
#include <utility>

// insert_or_assign keeps the last registration if called with the same
// name twice, which is convenient for config hot-reload in later phases.
namespace runtime::registry {

bool ServiceRegistry::Register(std::string name,
                               runtime::upstream::Upstream upstream) {
  if (name.empty()) {
    return false;
  }

  table_.insert_or_assign(std::move(name), std::move(upstream));
  return true;
}

runtime::upstream::Upstream *
ServiceRegistry::Get(std::string_view name) noexcept {
  auto it = table_.find(std::string(name));
  if (it == table_.end()) {
    return nullptr;
  }
  return &it->second;
}

const runtime::upstream::Upstream *ServiceRegistry::Get(std::string_view name) const noexcept {
  auto it = table_.find(std::string(name));
  if (it == table_.end()) {
    return nullptr;
  }
  return &it->second;
}
} // namespace runtime::registry
