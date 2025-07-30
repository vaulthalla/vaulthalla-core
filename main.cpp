#include "services/Vaulthalla.hpp"
#include "config/ConfigRegistry.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "database/Transactions.hpp"
#include "services/ServiceManager.hpp"
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
        ConfigRegistry::init(loadConfig("/etc/vaulthalla/config.yaml"));
        ThreadPoolRegistry::instance().init();
        Transactions::init();
        const auto serviceManager = std::make_shared<ServiceManager>();

        Filesystem::init(serviceManager->storageManager);
        serviceManager->storageManager->initStorageEngines();
        serviceManager->initServices();

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        while (!shouldExit) std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "[*] Shutting down Vaulthalla services..." << std::endl;

        serviceManager->syncController->stop();
        serviceManager->fuseService->stop();
        serviceManager->vaulthallaService->stop();
        ThreadPoolRegistry::instance().shutdown();

        std::cout << "[✓] Vaulthalla services shut down cleanly." << std::endl;

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "[-] Failed to initialize Vaulthalla: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
