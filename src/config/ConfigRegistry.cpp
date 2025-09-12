#include "config/ConfigRegistry.hpp"

#include <stdexcept>

namespace vh::config {

void ConfigRegistry::init(const std::filesystem::path& path) {
    std::call_once(init_flag_, [&]() {
        config_ = loadConfig(path);
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
