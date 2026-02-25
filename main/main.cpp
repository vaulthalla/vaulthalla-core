// Services
#include "../include/protocols/ProtocolService.hpp"
#include "runtime/Manager.hpp"
#include "runtime/Deps.hpp"

// Database
#include "database/Transactions.hpp"
#include "database/queries/UserQueries.hpp"
#include "database/queries/SyncEventQueries.hpp"

// Storage
#include "storage/Manager.hpp"
#include "fs/Filesystem.hpp"

// Seed
#include "seed/include/seed_db.hpp"
#include "seed/include/init_db_tables.hpp"

// Misc
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "log/Registry.hpp"

// Libraries
#include <csignal>
#include <pdfium/fpdfview.h>

using namespace vh::config;
using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::fs;

namespace {
std::atomic shouldExit = false;

void signalHandler(const int signum) {
    vh::log::Registry::vaulthalla()->info("[!] Signal {} received. Shutting down gracefully...", std::to_string(signum));
    shouldExit = true;
}
}

int main() {
    try {
        ConfigRegistry::init();
        vh::log::Registry::init();

        FPDF_LIBRARY_CONFIG config;
        config.version = 3;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);

        vh::log::Registry::vaulthalla()->info("[*] Initializing Vaulthalla services...");

        ThreadPoolManager::instance().init();

        vh::log::Registry::vaulthalla()->info("[*] Initializing services...");
        Transactions::init();
        seed::init_tables_if_not_exists();
        Transactions::dbPool_->initPreparedStatements();
        if (!UserQueries::adminUserExists()) vh::seed::seed_database();

        vh::log::Registry::vaulthalla()->info("[*] Initializing service dependencies...");
        vh::runtime::Deps::init();
        vh::runtime::Deps::setSyncController(vh::runtime::Manager::instance().getSyncController());
        vh::log::Registry::vaulthalla()->info("[✓] SyncController set in vh::runtime::Deps.");
        Filesystem::init(vh::runtime::Deps::get().storageManager);
        vh::runtime::Deps::get().storageManager->initStorageEngines();

        vh::log::Registry::vaulthalla()->info("[✓] Vaulthalla services initialized, starting...");
        vh::runtime::Manager::instance().startAll();

        vh::log::Registry::vaulthalla()->info("[*] Vaulthalla services started successfully.");

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        while (!shouldExit) std::this_thread::sleep_for(std::chrono::seconds(1));

        vh::log::Registry::vaulthalla()->info("[*] Shutting down Vaulthalla services...");

        vh::runtime::Manager::instance().stopAll(SIGTERM);
        ThreadPoolManager::instance().shutdown();
        FPDF_DestroyLibrary();

        vh::log::Registry::vaulthalla()->info("[✓] Vaulthalla services shut down cleanly.");

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        vh::log::Registry::vaulthalla()->error("[-] Failed to initialize Vaulthalla: {}", e.what());
        return EXIT_FAILURE;
    }
}
