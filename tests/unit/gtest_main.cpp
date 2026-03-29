#include <gtest/gtest.h>
#include <filesystem>
#include <paths.h>

#include "config/Registry.hpp"
#include "log/Registry.hpp"
#include "db/Transactions.hpp"
#include "seed/include/init_db_tables.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    try {
        vh::paths::enableTestMode();
        vh::paths::setLogPathForTesting();

        vh::config::Registry::init();
        vh::log::Registry::init();

        vh::db::Transactions::init();
        vh::db::seed::wipe_all_data_restart_identity();
        vh::db::seed::init_tables_if_not_exists();
        vh::db::Transactions::dbPool_->initPreparedStatements();

    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Vaulthalla test environment: " << e.what() << std::endl;
        return 1;
    }

    return RUN_ALL_TESTS();
}
