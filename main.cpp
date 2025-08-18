// Services
#include "services/Vaulthalla.hpp"
#include "services/ServiceManager.hpp"
#include "services/ServiceDepsRegistry.hpp"

// Database
#include "database/Transactions.hpp"
#include "database/Queries/UserQueries.hpp"

// Storage
#include "storage/StorageManager.hpp"
#include "storage/Filesystem.hpp"

// Seed
#include "seed/include/seed_db.hpp"
#include "seed/include/init_db_tables.hpp"

// Misc
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "include/services/LogRegistry.hpp"

// Libraries
#include <csignal>
#include <pdfium/fpdfview.h>

using namespace vh::config;
using namespace vh::services;
using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::logging;

namespace {
std::atomic shouldExit = false;

void signalHandler(const int signum) {
    LogRegistry::vaulthalla()->info("[!] Signal {} received. Shutting down gracefully...", std::to_string(signum));
    shouldExit = true;
}
}

int main() {
    try {
        ConfigRegistry::init(loadConfig("/etc/vaulthalla/config.yaml"));
        LogRegistry::init(ConfigRegistry::get().logging.log_dir);

        FPDF_LIBRARY_CONFIG config;
        config.version = 3;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);

        LogRegistry::vaulthalla()->info("[*] Initializing Vaulthalla services...");

        ThreadPoolManager::instance().init();

        LogRegistry::vaulthalla()->info("[*] Initializing services...");
        Transactions::init();
        seed::init_tables_if_not_exists();
        Transactions::dbPool_->initPreparedStatements();
        if (!UserQueries::adminUserExists()) vh::seed::init();

        LogRegistry::vaulthalla()->info("[*] Initializing service dependencies...");
        ServiceDepsRegistry::init();
        ServiceDepsRegistry::setSyncController(ServiceManager::instance().getSyncController());
        Filesystem::init(ServiceDepsRegistry::instance().storageManager);
        ServiceDepsRegistry::instance().storageManager->initStorageEngines();

        LogRegistry::vaulthalla()->info("[✓] Vaulthalla services initialized, starting...");
        ServiceManager::instance().startAll();

        LogRegistry::vaulthalla()->info("[*] Vaulthalla services started successfully.");

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        while (!shouldExit) std::this_thread::sleep_for(std::chrono::seconds(1));

        LogRegistry::vaulthalla()->info("[*] Shutting down Vaulthalla services...");

        ServiceManager::instance().stopAll(SIGTERM);
        ThreadPoolManager::instance().shutdown();
        FPDF_DestroyLibrary();

        LogRegistry::vaulthalla()->info("[✓] Vaulthalla services shut down cleanly.");

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        LogRegistry::vaulthalla()->error("[-] Failed to initialize Vaulthalla: {}", e.what());
        return EXIT_FAILURE;
    }
}
