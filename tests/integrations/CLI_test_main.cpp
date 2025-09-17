#include "IntegrationsTestRunner.hpp"
#include "database/Transactions.hpp"
#include "database/Queries/UserQueries.hpp"
#include "seed/include/init_db_tables.hpp"
#include "seed/include/seed_db.hpp"
#include "config/ConfigRegistry.hpp"
#include "logging/LogRegistry.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "services/ServiceManager.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/Filesystem.hpp"
#include "storage/StorageManager.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <chrono>
#include <paths.h>

namespace fs = std::filesystem;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::config;
using namespace vh::logging;
using namespace vh::concurrency;
using namespace vh::services;
using namespace vh::storage;
using namespace vh::test::cli;

static void initBase() {
    vh::paths::enableTestMode();
    ConfigRegistry::init();
    LogRegistry::init(fs::temp_directory_path() / "vaulthalla-test");
    ThreadPoolManager::instance().init();
}

static void initDB() {
    vh::database::Transactions::init();
    vh::database::seed::wipe_all_data_restart_identity();
    vh::database::seed::init_tables_if_not_exists();
    vh::database::Transactions::dbPool_->initPreparedStatements();
    vh::seed::seed_database();
}

static void initServices() {
    ServiceDepsRegistry::init();
    ServiceDepsRegistry::setSyncController(ServiceManager::instance().getSyncController());
    Filesystem::init(ServiceDepsRegistry::instance().storageManager);
    ServiceDepsRegistry::instance().storageManager->initStorageEngines();
    ServiceManager::instance().startTestServices();
}

static void ensureAdminExists() {
    if (!vh::database::UserQueries::adminUserExists()) {
        LogRegistry::vaulthalla()->error("No admin user found; cannot run CLI tests");
        exit(1);
    }
}

static int runTests() {
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    IntegrationsTestRunner runner(CLITestConfig::Default());
    const int exit_status = runner() == 0 ? 0 : 1;

    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "CLI tests completed in " << static_cast<double>(duration) / 1000.0 << " seconds" << std::endl;

    return exit_status;
}

static void shutdown() {
    ServiceManager::instance().stopAll();
    ThreadPoolManager::instance().shutdown();
}

int main() {
    initBase();
    initDB();
    initServices();
    ensureAdminExists();
    const auto exit_status = runTests();
    shutdown();
    exit(exit_status);
}
