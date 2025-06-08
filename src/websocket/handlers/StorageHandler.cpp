#include "websocket/handlers/StorageHandler.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "storage/StorageManager.hpp"
#include "websocket/WebSocketSession.hpp"

#include <iostream>

namespace vh::websocket {

    StorageHandler::StorageHandler(const std::shared_ptr<vh::storage::StorageManager> &storageManager)
            : storageManager_(storageManager) {}

    void StorageHandler::handleInitLocal(const json& msg, WebSocketSession& session) {
        try {
            std::string mountName = msg.at("mountName").get<std::string>();
            std::string rootPath = msg.at("rootPath").get<std::string>();

            auto backend = std::make_shared<vh::storage::LocalDiskStorageEngine>(rootPath);

            storageManager_->mountLocal(mountName, std::filesystem::path(rootPath));

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

            storageManager_->mountLocal(mountName, std::filesystem::path(msg.at("rootPath").get<std::string>()));

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

            // TODO: Implement unmount logic

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

} // namespace vh::websocket
