#include "services/Vaulthalla.hpp"
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "database/Transactions.hpp"
#include "services/ServiceManager.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "storage/Filesystem.hpp"

#include <iostream>
#include <signal.h>

using namespace vh::config;
using namespace vh::services;
using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;

namespace {
std::atomic shouldExit = false;

void signalHandler(int signum) {
    std::cout << "\n[!] Signal " << signum << " received. Shutting down gracefully..." << std::endl;
    shouldExit = true;
}
}

int main() {
    try {
        std::cout << "[*] Initializing Vaulthalla services..." << std::endl;
        ConfigRegistry::init(loadConfig("/etc/vaulthalla/config.yaml"));
        ThreadPoolRegistry::instance().init();
        Transactions::init();
        ServiceDepsRegistry::init();
        ServiceDepsRegistry::setSyncController(ServiceManager::instance().getSyncController());

        std::cout << "[*] Starting services..." << std::endl;
        Filesystem::init(ServiceDepsRegistry::instance().storageManager);
        ServiceDepsRegistry::instance().storageManager->initStorageEngines();
        ServiceManager::instance().startAll();

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        std::cout << "[✓] Vaulthalla services initialized successfully." << std::endl;

        while (!shouldExit) std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "[*] Shutting down Vaulthalla services..." << std::endl;

        ThreadPoolRegistry::instance().shutdown();
        ServiceManager::instance().stopAll(SIGTERM);

        std::cout << "[✓] Vaulthalla services shut down cleanly." << std::endl;

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "[-] Failed to initialize Vaulthalla: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
