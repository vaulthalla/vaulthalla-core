#include <gtest/gtest.h>
#include <filesystem>

#include "config/ConfigRegistry.hpp"
#include "logging/LogRegistry.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    try {
        vh::config::ConfigRegistry::init();
        const auto logPath = fs::temp_directory_path() / "vaulthalla-test";
        vh::logging::LogRegistry::init(logPath);

    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Vaulthalla test environment: " << e.what() << std::endl;
        return 1;
    }

    return RUN_ALL_TESTS();
}
