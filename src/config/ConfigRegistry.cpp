#include "config/ConfigRegistry.hpp"

#include <stdexcept>
#include <paths.h>

namespace vh::config {

void ConfigRegistry::init() {
    std::call_once(init_flag_, [&]() {
        config_ = loadConfig(paths::getConfigPath());
        initialized_ = true;
    });
}

const Config& ConfigRegistry::get() {
    ensureInitialized();
    return config_;
}

void ConfigRegistry::ensureInitialized() {
    if (!initialized_)
        throw std::runtime_error("ConfigRegistry accessed before initialization. Call ConfigRegistry::init() first.");
}

} // namespace vh::config
