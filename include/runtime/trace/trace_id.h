#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace runtime::trace {

// 128-bit random trace identifier for log correlation.
// Generated from /dev/urandom; safe to copy and pass by value.
struct TraceId {
    std::array<uint8_t, 16> bytes{};

    // Returns a newly generated random TraceId.
    static TraceId Generate();

    // Returns a zero TraceId (invalid sentinel).
    static TraceId Zero() noexcept { return TraceId{}; }

    bool IsZero() const noexcept;

    // Returns lowercase hex string: "a1b2c3d4e5f6...".
    std::string ToHex() const;

    bool operator==(const TraceId& other) const noexcept {
        return bytes == other.bytes;
    }
    bool operator!=(const TraceId& other) const noexcept {
        return bytes != other.bytes;
    }
};

}  // namespace runtime::trace
