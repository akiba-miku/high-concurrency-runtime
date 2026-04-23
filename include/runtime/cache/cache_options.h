#pragma once

#include <cstddef>

namespace runtime::cache {

struct CacheOptions {
    std::size_t capacity = 1024;
    std::size_t segment_count = 16;
    double default_ttl_sec = 0.0;
    bool sliding = false;
    bool stats_enabled = true;
};

} // namespace runtime::cache