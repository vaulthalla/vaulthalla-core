#include "services/FUSELoopRunner.hpp"
#include "config/ConfigRegistry.hpp"
#include "FUSECmdRouter.hpp"
#include "types/FUSECommand.hpp"
#include "services/SyncController.hpp"
#include "services/ThreadPoolRegistry.hpp"
#include "concurrency/SharedThreadPoolRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "database/Transactions.hpp"

#include <iostream>
#include <thread>

using namespace vh::fuse;
using namespace vh::config;
using namespace vh::services;
using namespace vh::storage;
using namespace vh::types::fuse;
using namespace vh::fuse::ipc;

int main(int argc, char* argv[]) {
    ConfigRegistry::init(loadConfig("/etc/vaulthalla/config.yaml"));
    const auto mountPath = ConfigRegistry::get().fuse.root_mount_path;
    if (!std::filesystem::exists(mountPath)) std::filesystem::create_directories(mountPath);

    vh::database::Transactions::init();

    ThreadPoolRegistry::instance().init();
    SharedThreadPoolRegistry::instance().init();

    const auto storageManager = std::make_shared<StorageManager>();
    const auto syncController = std::make_shared<SyncController>(storageManager);
    syncController->start();

    const auto router = std::make_unique<CommandRouter>("/tmp/vaulthalla.sock");

    router->setCommandHandler([&syncController](const FUSECommand& cmd) {
        if (cmd.type == CommandType::SYNC) {
            std::cout << "[+] Received sync command for vault " << cmd.vaultId << "\n";
            syncController->runNow(cmd.vaultId);
        } else {
            std::cerr << "[-] Unsupported command type: " << static_cast<int>(cmd.type) << "\n";
        }
    });

    std::thread listener([&]() { router->start(); });

    FUSELoopRunner loop(storageManager);
    if (!loop.run()) {
        std::cerr << "[-] FUSE loop failed.\n";
        return 1;
    }

    listener.join();
    return 0;
}
