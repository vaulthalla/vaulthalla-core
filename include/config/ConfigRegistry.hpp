#pragma once

#include "config/Config.hpp"
#include <mutex>

namespace vh::config {

class ConfigRegistry {
public:
    static void init(const Config& cfg);
    static const Config& get();

private:
    static void ensureInitialized();

    static inline Config config_;
    static inline bool initialized_ = false;
    static inline std::once_flag init_flag_;
};

} // namespace vh::config
