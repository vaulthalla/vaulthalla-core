#include "services/Vaulthalla.hpp"
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "database/Transactions.hpp"
#include "database/Queries/UserQueries.hpp"
#include "services/ServiceManager.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "storage/Filesystem.hpp"
#include "logging/LogRegistry.hpp"
#include "util/initdb.hpp"

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

void signalHandler(int signum) {
    LogRegistry::vaulthalla()->info("[!] Signal {} received. Shutting down gracefully...", std::to_string(signum));
    shouldExit = true;
}
}

int main() {
    try {
        ConfigRegistry::init(loadConfig("/etc/vaulthalla/config.yaml"));
        LogRegistry::init(ConfigRegistry::get().logging.log_dir);
        const auto log = LogRegistry::vaulthalla();

        FPDF_LIBRARY_CONFIG config;
        config.version = 3;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);

        log->info("[*] Initializing Vaulthalla services...");

        ThreadPoolManager::instance().init();
        Transactions::init();

        if (!UserQueries::adminUserExists()) vh::seed::init();
        // if (ConfigRegistry::get().advanced.dev_mode) vh::seed::initDevCloudVault();

        ServiceDepsRegistry::init();
        ServiceDepsRegistry::setSyncController(ServiceManager::instance().getSyncController());
        Filesystem::init(ServiceDepsRegistry::instance().storageManager);
        ServiceDepsRegistry::instance().storageManager->initStorageEngines();
        ServiceManager::instance().startAll();

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        log->info("[✓] Vaulthalla services initialized successfully.");

        while (!shouldExit) std::this_thread::sleep_for(std::chrono::seconds(1));

        log->info("[*] Shutting down Vaulthalla services...");

        ServiceManager::instance().stopAll(SIGTERM);
        ThreadPoolManager::instance().shutdown();
        FPDF_DestroyLibrary();

        log->info("[✓] Vaulthalla services shut down cleanly.");

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        LogRegistry::vaulthalla()->error("[-] Failed to initialize Vaulthalla: {}", e.what());
        return EXIT_FAILURE;
    }
}
