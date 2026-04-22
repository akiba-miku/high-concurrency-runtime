#include "runtime/trace/trace_id.h"

#include <cstdio>
#include <fstream>

namespace runtime::trace {

TraceId TraceId::Generate() {
    TraceId id;
    // Read 16 random bytes from /dev/urandom.
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom) {
        urandom.read(reinterpret_cast<char*>(id.bytes.data()),
                     static_cast<std::streamsize>(id.bytes.size()));
    }
    return id;
}

bool TraceId::IsZero() const noexcept {
    for (auto b : bytes) {
        if (b != 0) return false;
    }
    return true;
}

std::string TraceId::ToHex() const {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(32);
    for (uint8_t b : bytes) {
        out += kHex[b >> 4];
        out += kHex[b & 0x0f];
    }
    return out;
}

}  // namespace runtime::trace
