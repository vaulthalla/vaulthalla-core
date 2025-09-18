#include <gtest/gtest.h>
#include <filesystem>
#include <paths.h>

#include "config/ConfigRegistry.hpp"
#include "logging/LogRegistry.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    try {
        vh::paths::setLogPathForTesting();
        vh::config::ConfigRegistry::init();
        vh::logging::LogRegistry::init();

    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Vaulthalla test environment: " << e.what() << std::endl;
        return 1;
    }

    return RUN_ALL_TESTS();
}
