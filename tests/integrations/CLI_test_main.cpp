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
#include "fs/Filesystem.hpp"
#include "storage/Manager.hpp"

#include <iostream>
#include <string>
#include <chrono>
#include <paths.h>

using namespace vh::shell;
using namespace vh::types;
using namespace vh::config;
using namespace vh::logging;
using namespace vh::concurrency;
using namespace vh::services;
using namespace vh::storage;
using namespace vh::test::cli;
using namespace vh::database;
using namespace vh::fs;

static void initBase() {
    vh::paths::enableTestMode();
    ConfigRegistry::init();
    LogRegistry::init();
    ThreadPoolManager::instance().init();
}

static void initDB() {
    Transactions::init();

    seed::wipe_all_data_restart_identity();
    seed::init_tables_if_not_exists();
    Transactions::dbPool_->initPreparedStatements();
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
    if (!UserQueries::adminUserExists()) {
        LogRegistry::vaulthalla()->error("No admin user found; cannot run CLI tests");
        exit(1);
    }
}

static int runTests() {
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    IntegrationsTestRunner runner(CLITestConfig::Medium());
    const int exit_status = runner();

    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "CLI tests completed in " << static_cast<double>(duration) / 1000.0 << " seconds" << std::endl;

    return exit_status;
}

// static void shutdown() {
//     ServiceManager::instance().stopAll();
//     ThreadPoolManager::instance().shutdown();
// }

int main() {
    initBase();
    initDB();
    initServices();
    ensureAdminExists();
    const auto exit_status = runTests();
    // dont call shutdown cause it will SIGTERM the thread and exit 1
    std::_Exit(exit_status);
}
