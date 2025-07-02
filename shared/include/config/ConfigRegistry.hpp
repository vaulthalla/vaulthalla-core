#pragma once

#include "types/config/Config.hpp"
#include <mutex>

namespace vh::types::config {

class ConfigRegistry {
public:
    static void init(const types::config::Config& cfg);
    static const types::config::Config& get();

private:
    static void ensureInitialized();

    static inline types::config::Config config_;
    static inline bool initialized_ = false;
    static inline std::once_flag init_flag_;
};

} // namespace vh::types::config
