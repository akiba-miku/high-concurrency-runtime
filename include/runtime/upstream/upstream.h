#pragma once

#include "runtime/upstream/backend.h"

#include <memory>
#include <string>
#include <vector>

namespace runtime::upstream {

// Upstream represents one logical service and owns all backend instances
// belonging to that service.
//
// Example:
//      "user_service" -> [127.0.0.1:9001, 127.0.0.1:9002]
//
// Ownership:
//  - Upstream exclusively owns Backend objects via std::unique_ptr
//  - Raw Backend* pointers obtained via Backends() remain valid for the
//    lifetime of this Upstream (the backing vector is append-only at startup).
//
// Copy vs move:
//  - Not copyable: Backend contains atomics; copying is semantically wrong.
//  - Movable: required so Upstream can be stored by value in the registry map.
class Upstream {
public:
public:
  explicit Upstream(std::string name) : name_(std::move(name)) {}
  ~Upstream() = default;

  Upstream(const Upstream&) = delete;
  Upstream& operator=(const Upstream&) = delete;
  Upstream(Upstream&&) = default;
  Upstream& operator=(Upstream&&) = default;

  // Takes ownership of a heap-allocated Backend.
  // Return false if backend is null or invalid
  bool AddBackend(std::unique_ptr<Backend> backend);

  // Convenience overload: constructs a Backend in place.
  bool AddBackend(std::string host, uint16_t port, int weight = 1);

  const std::string &Name() const { return name_; }

  const std::vector<std::unique_ptr<Backend>> &Backends() const noexcept {
    return backends_;
  }

  bool Empty() const noexcept { return backends_.empty(); }
  std::size_t Size() const noexcept { return backends_.size(); }
  int HealthyCount() const noexcept;

private:
  std::string name_;
  std::vector<std::unique_ptr<Backend>> backends_;
};

} // namespace runtime::upstream
