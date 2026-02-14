#pragma once

#include <cstdint>
#include <ctime>

namespace vh::types::sync {

struct ScopedOp {
    uint64_t size_bytes{};
    std::time_t timestamp_begin{};
    std::time_t timestamp_end{};

    void start();
    void start(uint64_t size_bytes);
    void stop();
    uint64_t duration_ms() const;
};

}
