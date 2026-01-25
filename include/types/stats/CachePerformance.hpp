#pragma once
#include <cstdint>

struct CachePerformance {
    uint64_t hits, misses, evictions;
    uintmax_t used, free, max;
    double hit_rate, thumbnail_hit_rate, thumbnail_misses, preview_generation_time_ms;
};
