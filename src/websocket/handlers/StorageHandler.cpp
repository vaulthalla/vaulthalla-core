#include "websocket/handlers/StorageHandler.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "websocket/WebSocketSession.hpp"
// #include "storage/S3Backend.hpp"
// #include "storage/R2Backend.hpp"

#include <iostream>

namespace vh::websocket {

    StorageHandler::StorageHandler() = default;

    void StorageHandler::handleInitLocal(const json& msg, WebSocketSession& session) {
        try {
            std::string mountName = msg.at("mountName").get<std::string>();
            std::string rootPath = msg.at("rootPath").get<std::string>();

            auto backend = std::make_shared<vh::storage::LocalDiskStorageEngine>(rootPath);

            {
                std::lock_guard<std::mutex> lock(mountsMutex_);
                mounts_[mountName] = backend;
            }

            json response = {
                    {"command", "storage.local.init.response"},
                    {"mountName", mountName},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Mounted local storage: " << mountName << " -> " << rootPath << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleInitLocal error: " << e.what() << "\n";
        }
    }

    void StorageHandler::handleInitS3(const json& msg, WebSocketSession& session) {
        // TODO: Implement S3Backend and plug here
        std::cerr << "[StorageHandler] handleInitS3 not implemented yet\n";
    }

    void StorageHandler::handleInitR2(const json& msg, WebSocketSession& session) {
        // TODO: Implement R2Backend and plug here
        std::cerr << "[StorageHandler] handleInitR2 not implemented yet\n";
    }

    void StorageHandler::handleMount(const json& msg, WebSocketSession& session) {
        try {
            std::string mountName = msg.at("mountName").get<std::string>();

            {
                std::lock_guard<std::mutex> lock(mountsMutex_);
                if (mounts_.find(mountName) == mounts_.end()) {
                    throw std::runtime_error("Mount not found: " + mountName);
                }
            }

            json response = {
                    {"command", "storage.mount.response"},
                    {"mountName", mountName},
                    {"status", "ok"}
            };

            session.send(response);
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleMount error: " << e.what() << "\n";
        }
    }

    void StorageHandler::handleUnmount(const json& msg, WebSocketSession& session) {
        try {
            std::string mountName = msg.at("mountName").get<std::string>();

            {
                std::lock_guard<std::mutex> lock(mountsMutex_);
                mounts_.erase(mountName);
            }

            json response = {
                    {"command", "storage.unmount.response"},
                    {"mountName", mountName},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[StorageHandler] Unmounted: " << mountName << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageHandler] handleUnmount error: " << e.what() << "\n";
        }
    }

    std::shared_ptr<vh::storage::StorageEngine> StorageHandler::getMount(const std::string& mountName) {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        auto it = mounts_.find(mountName);
        if (it != mounts_.end()) {
            return it->second;
        }
        throw std::runtime_error("Mount not found: " + mountName);
    }

} // namespace vh::websocket
