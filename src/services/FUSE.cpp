#include "services/FUSELoopRunner.hpp"
#include "config/ConfigRegistry.hpp"
#include "services/SyncController.hpp"
#include "services/ThreadPoolRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "storage/Filesystem.hpp"
#include "database/Transactions.hpp"

#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

using namespace vh::fuse;
using namespace vh::config;
using namespace vh::services;
using namespace vh::storage;

namespace {
std::atomic shouldExit = false;

void signalHandler(int signum) {
    std::cout << "\n[!] Signal " << signum << " received. Shutting down gracefully..." << std::endl;
    shouldExit = true;
}
}

int main(int argc, char* argv[]) {
    std::cout << "[*] Bootstrapping Vaulthalla native FUSE daemon..." << std::endl;

    ConfigRegistry::init(loadConfig("/etc/vaulthalla/config.yaml"));
    const auto mountPath = ConfigRegistry::get().fuse.root_mount_path;

    vh::database::Transactions::init();
    ThreadPoolRegistry::instance().init();

    const auto storageManager = std::make_shared<StorageManager>();
    const auto syncController = std::make_shared<SyncController>(storageManager);
    const auto fuseLoop = std::make_shared<FUSELoopRunner>(storageManager);
    Filesystem::init(storageManager);

    try { fuseLoop->run(); } catch (const std::exception& e) {
        std::cerr << "[-] Failed to start FUSE loop: " << e.what() << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    std::cout << "[+] Initializing storage engines..." << std::endl;
    storageManager->initStorageEngines();

    std::cout << "[+] Initializing syncController..." << std::endl;
    syncController->start();

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    while (!shouldExit) std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "[*] Beginning shutdown of Vaulthalla..." << std::endl;

    fuseLoop->stop();
    ThreadPoolRegistry::instance().shutdown();

    std::cout << "[✓] Vaulthalla daemon shut down cleanly." << std::endl;
    return 0;
}