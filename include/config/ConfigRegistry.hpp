#pragma once

#include "config/Config.hpp"

#include <mutex>
#include <paths.h>

namespace vh::config {

class ConfigRegistry {
public:
    static void init(const std::filesystem::path& path = paths::getConfigPath());
    static const Config& get();

private:
    static void ensureInitialized();

    static inline Config config_;
    static inline bool initialized_ = false;
    static inline std::once_flag init_flag_;
};

} // namespace vh::config
