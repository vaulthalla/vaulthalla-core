#include "FUSECmdRouter.hpp"
#include "FUSEMountManager.hpp"
#include "FUSEOperations.hpp"
#include "FUSEPermissions.hpp"
#include "fuse/StorageBridge/RemoteFSProxy.hpp"
#include "fuse/StorageBridge/UnifiedStorage.hpp"
#include "fuse/Command.hpp"
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    const std::string mountPath = "/mnt/vaulthalla";
    vh::fuse::FUSEMountManager mountManager(mountPath);

    if (!mountManager.mount()) {
        std::cerr << "[-] Could not prepare mountpoint.\n";
        return 1;
    }

    auto storage = std::make_shared<vh::shared::bridge::UnifiedStorage>();
    auto proxy = std::make_shared<vh::shared::bridge::RemoteFSProxy>(storage);
    vh::fuse::bind(proxy); // Set it for FUSE ops

    const auto router = std::make_unique<vh::fuse::ipc::CommandRouter>("/tmp/vaulthalla.sock");

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

    auto permissions = std::make_unique<vh::fuse::FUSEPermissions>();
    vh::fuse::bindPermissions(std::move(permissions));

    struct fuse_operations ops = vh::fuse::getOperations();
    std::vector<char*> args(argv, argv + argc);
    args.push_back(const_cast<char*>(mountPath.c_str()));

    return fuse_main(static_cast<int>(args.size()), args.data(), &ops, nullptr);
}
