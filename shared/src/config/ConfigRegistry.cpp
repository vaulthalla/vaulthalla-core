#include "types/config/ConfigRegistry.hpp"
#include <stdexcept>

namespace vh::types::config {

void ConfigRegistry::init(const types::config::Config& cfg) {
    std::call_once(init_flag_, [&]() {
        config_ = cfg;
        initialized_ = true;
    });
}

const types::config::Config& ConfigRegistry::get() {
    ensureInitialized();
    return config_;
}

void ConfigRegistry::ensureInitialized() {
    if (!initialized_) {
        throw std::runtime_error("ConfigRegistry accessed before initialization. Call ConfigRegistry::init() first.");
    }
}

} // namespace vh::types::config
