#include "FUSECmdRouter.hpp"
#include "FUSEMountManager.hpp"
#include "FUSEOperations.hpp"
#include "FUSEPermissions.hpp"
#include "fuse/StorageBridge/RemoteFSProxy.hpp"
#include "fuse/StorageBridge/UnifiedStorage.hpp"
#include "fuse/Command.hpp"
#include <iostream>
#include <thread>

using namespace vh::fuse;

int main(const int argc, char* argv[]) {
    const std::string mountPath = "/mnt/vaulthalla";
    FUSEMountManager mountManager(mountPath);

    if (!mountManager.mount()) {
        std::cerr << "[-] Could not prepare mountpoint.\n";
        return 1;
    }

    auto storage = std::make_shared<vh::shared::bridge::UnifiedStorage>();
    auto proxy = std::make_shared<vh::shared::bridge::RemoteFSProxy>(storage);
    bind(proxy); // Set it for FUSE ops

    const auto router = std::make_unique<ipc::CommandRouter>("/tmp/vaulthalla.sock");

    router->setCommandHandler([proxy](const vh::types::fuse::Command& cmd) {
        using vh::types::fuse::CommandType;
        std::cout << "[router] Command: " << to_string(cmd.type) << " | " << cmd.path << "\n";

        try {
            switch (cmd.type) {
            case CommandType::CREATE:
                if (!cmd.mode) throw std::runtime_error("Missing mode");
                proxy->createFile(cmd.path, *cmd.mode);
                break;

            case CommandType::DELETE:
                proxy->deleteFile(cmd.path);
                break;

            case CommandType::MKDIR:
                if (!cmd.mode) throw std::runtime_error("Missing mode");
                proxy->mkdir(cmd.path, *cmd.mode);
                break;

            case CommandType::RMDIR:
                proxy->deleteDirectory(cmd.path);
                break;

            case CommandType::RENAME:
                if (!cmd.newPath) throw std::runtime_error("Missing newPath");
                proxy->rename(cmd.path, *cmd.newPath);
                break;

            case CommandType::CHMOD:
                if (!cmd.mode) throw std::runtime_error("Missing mode");
                proxy->setPermissions(cmd.path, *cmd.mode);
                break;

            case CommandType::CHOWN:
                if (!cmd.uid || !cmd.gid) throw std::runtime_error("Missing uid/gid");
                proxy->setOwnership(cmd.path, *cmd.uid, *cmd.gid);
                break;

            case CommandType::TRUNCATE:
                if (!cmd.size) throw std::runtime_error("Missing size");
                proxy->resizeFile(cmd.path, *cmd.size);
                break;

            case CommandType::TOUCH: {
                time_t now = std::time(nullptr);
                proxy->updateTimestamps(cmd.path, now, now);
                break;
            }

            case CommandType::SYNC:
                proxy->sync(cmd.path);
                break;

            case CommandType::EXISTS:
                if (proxy->fileExists(cmd.path)) {
                    std::cout << "[router] EXISTS: " << cmd.path << " exists.\n";
                } else {
                    std::cout << "[router] EXISTS: " << cmd.path << " does not exist.\n";
                }
                break;

            case CommandType::STAT:
                proxy->stat(cmd.path); // implement as needed
                break;

            case CommandType::LISTDIR:
                proxy->listDir(cmd.path); // implement as needed
                break;

            case CommandType::FLUSH:
                proxy->flush(cmd.path); // if you’re tracking handles
                break;

            case CommandType::READ:
                // proxy->readFile(cmd.path); // or pipe data if designed
                break;

            case CommandType::WRITE:
                throw std::runtime_error("WRITE over control socket not supported (use file IO)");
                break;

            default:
                std::cerr << "[router] Unknown or unhandled command type: " << to_string(cmd.type) << "\n";
            }
        } catch (const std::exception& ex) {
            std::cerr << "[router] Command failed: " << ex.what() << "\n";
        }
    });

    std::thread listener([&]() { router->start(); });

    auto permissions = std::make_unique<FUSEPermissions>();
    bindPermissions(std::move(permissions));

    std::vector<std::string> fuseArgsStr = {
        argv[0], // program name
        "-f",    // foreground mode
        "-o",
        "big_writes,max_readahead=1048576,auto_cache,attr_timeout=60,entry_timeout=60,negative_timeout=10"
    };

    // Add the mountpoint last
    fuseArgsStr.push_back(mountPath);

    // Convert to `char*` array
    std::vector<char*> fuseArgs;
    for (auto& s : fuseArgsStr)
        fuseArgs.push_back(s.data());

    const fuse_operations ops = getOperations();

    return fuse_main(static_cast<int>(fuseArgs.size()), fuseArgs.data(), &ops, nullptr);
}
