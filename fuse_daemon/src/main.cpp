#include "services/FUSELoopRunner.hpp"
#include "config/ConfigRegistry.hpp"
#include "control/FUSECmdRouter.hpp"
#include "types/FUSECommand.hpp"
#include "services/SyncController.hpp"
#include "services/ThreadPoolRegistry.hpp"
#include "concurrency/SharedThreadPoolRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "database/Transactions.hpp"

#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

using namespace vh::fuse;
using namespace vh::config;
using namespace vh::services;
using namespace vh::storage;
using namespace vh::types::fuse;
using namespace vh::fuse::ipc;

namespace {
    std::atomic shouldExit = false;

    void signalHandler(int signum) {
        std::cout << "\n[!] Signal " << signum << " received. Shutting down gracefully...\n";
        shouldExit = true;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "[*] Bootstrapping Vaulthalla native FUSE daemon...\n";

    ConfigRegistry::init(loadConfig("/etc/vaulthalla/config.yaml"));
    const auto mountPath = ConfigRegistry::get().fuse.root_mount_path;
    if (!std::filesystem::exists(mountPath)) {
        std::cout << "[+] Creating mountpoint at: " << mountPath << "\n";
        std::filesystem::create_directories(mountPath);
    }

    vh::database::Transactions::init();
    ThreadPoolRegistry::instance().init();
    SharedThreadPoolRegistry::instance().init();

    const auto storageManager = std::make_shared<StorageManager>();
    const auto syncController = std::make_shared<SyncController>(storageManager);
    const auto fuseLoop = std::make_shared<FUSELoopRunner>(storageManager);

    try { fuseLoop->run(); }
    catch (const std::exception& e) {
        std::cerr << "[-] Failed to start FUSE loop: " << e.what() << "\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    syncController->start();

    const auto router = std::make_unique<CommandRouter>("/tmp/vaulthalla.sock");

    router->setCommandHandler([&syncController](const FUSECommand& cmd) {
        switch (cmd.type) {
            case CommandType::SYNC:
                std::cout << "[+] SYNC command received for vault: " << cmd.vaultId << "\n";
                syncController->runNow(cmd.vaultId);
                break;
            default:
                std::cerr << "[-] Unsupported command type: " << static_cast<int>(cmd.type) << "\n";
                break;
        }
    });

    std::thread listener([&]() { router->start(); });

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    while (!shouldExit) std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "[*] Beginning shutdown of Vaulthalla...\n";

    router->stop();
    listener.join();
    fuseLoop->stop();

    ThreadPoolRegistry::instance().shutdown();
    SharedThreadPoolRegistry::instance().shutdown();

    std::cout << "[✓] Vaulthalla daemon shut down cleanly.\n";
    return 0;
}
