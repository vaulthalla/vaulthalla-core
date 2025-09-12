#include "CLITestRunner.hpp"
#include "CLITestContext.hpp"
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

#include <pdfium/fpdfview.h>
#include <memory>
#include <filesystem>
#include <iostream>
#include <string>
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

int main() {
    vh::paths::enableTestMode();
    ConfigRegistry::init("config.yaml");
    LogRegistry::init(fs::temp_directory_path() / "vaulthalla-test");

    FPDF_LIBRARY_CONFIG config;
    config.version = 3;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);

    ThreadPoolManager::instance().init();

    vh::database::Transactions::init();
    vh::database::seed::wipe_all_data_restart_identity();
    vh::database::seed::init_tables_if_not_exists();
    vh::database::Transactions::dbPool_->initPreparedStatements();
    vh::seed::seed_database();

    ServiceDepsRegistry::init();
    ServiceDepsRegistry::setSyncController(ServiceManager::instance().getSyncController());

    const auto mountPoint = fs::temp_directory_path() / "vaulthalla-test-mnt";
    ServiceManager::instance().setFuseMountPoint(mountPoint);

    Filesystem::init(ServiceDepsRegistry::instance().storageManager);
    ServiceDepsRegistry::instance().storageManager->initStorageEngines();
    ServiceManager::instance().startTestServices();

    if (!vh::database::UserQueries::getUserByName("admin")) {
        LogRegistry::vaulthalla()->error("No admin user found; cannot run CLI tests");
        return 1;
    }

    CLITestRunner runner(CLITestConfig::Default());
    const int exit_status = runner() == 0 ? 0 : 1;

    ServiceManager::instance().stopAll(SIGKILL);
    ThreadPoolManager::instance().shutdown();
    FPDF_DestroyLibrary();

    exit(exit_status);
}
