#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "security/PermissionManager.hpp"
#include "storage/StorageManager.hpp"
#include "auth/User.hpp"
#include <iostream>

namespace vh::websocket {

    FileSystemHandler::FileSystemHandler(std::shared_ptr<vh::storage::StorageManager> storageHandler)
            : storageManager_(std::move(storageHandler)) {}

    FileSystemHandler::FileSystemHandler(std::shared_ptr<vh::storage::StorageManager> storageHandler,
                                         std::shared_ptr<vh::security::PermissionManager> permissionManager)
            : storageManager_(std::move(storageHandler)),
              permissionManager_(std::move(permissionManager)) {}

    void FileSystemHandler::validateAuth(WebSocketSession& session) {
        if (!session.getAuthenticatedUser()) {
            throw std::runtime_error("Unauthorized");
        }
    }

    void FileSystemHandler::enforcePermission(WebSocketSession& session,
                                              const std::string& mountName,
                                              const std::string& path,
                                              const std::string& requiredPermission) {
        auto user = session.getAuthenticatedUser();
        if (!user) {
            throw std::runtime_error("Unauthorized");
        }

        const std::string& username = user->getUsername();

        bool allowed = permissionManager_->hasPermission(username, mountName, path, requiredPermission);
        if (!allowed) {
            throw std::runtime_error("Permission denied: " + requiredPermission + " for " + path);
        }
    }

    void FileSystemHandler::handleListDir(const json& msg, WebSocketSession& session) {
        try {
            validateAuth(session);

            std::string mountName = msg.at("mountName").get<std::string>();
            std::string path = msg.at("path").get<std::string>();

            json entries;

            // Try Local first
            try {
                auto fsManager = storageManager_->getLocalEngine(mountName);
                auto files = fsManager->listFilesInDir(path);

                entries = json::array();
                for (const auto& file : files) {
                    entries.push_back(file.string());
                }
            } catch (...) {
                // Try Cloud
                auto cloudProvider = storageManager_->getCloudEngine(mountName);
                entries = cloudProvider->storage::StorageEngine::listFilesInDir(path);
            }

            json response = {
                    {"command", "fs.listDir.response"},
                    {"status", "ok"},
                    {"mountName", mountName},
                    {"path", path},
                    {"entries", entries}
            };

            session.send(response);

            std::cout << "[FileSystemHandler] ListDir on mount '" << mountName << "' path '" << path << "'\n";

        } catch (const std::exception& e) {
            std::cerr << "[FileSystemHandler] handleListDir error: " << e.what() << "\n";

            json response = {
                    {"command", "fs.listDir.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void FileSystemHandler::handleReadFile(const json& msg, WebSocketSession& session) {
        try {
            validateAuth(session);

            std::string mountName = msg.at("mountName").get<std::string>();
            std::string path = msg.at("path").get<std::string>();

            std::string fileContent;

            try {
                auto fsManager = storageManager_->getLocalEngine(mountName);
                auto dataOpt = fsManager->readFile(path);
                if (!dataOpt.has_value()) throw std::runtime_error("File not found");

                // Encode binary data as base64 string (TODO: implement your base64 encode)
                fileContent = std::string(dataOpt->begin(), dataOpt->end());
            } catch (...) {
                auto cloudProvider = storageManager_->getCloudEngine(mountName);
                auto dataOpt = cloudProvider->readFile(path);
                if (!dataOpt.has_value()) throw std::runtime_error("File not found");

                // Encode binary data as base64 string (TODO: implement your base64 encode)
                fileContent = std::string(dataOpt->begin(), dataOpt->end());
            }

            json response = {
                    {"command", "fs.readFile.response"},
                    {"status", "ok"},
                    {"mountName", mountName},
                    {"path", path},
                    {"data", fileContent}
            };

            session.send(response);

            std::cout << "[FileSystemHandler] ReadFile on mount '" << mountName << "' path '" << path << "'\n";

        } catch (const std::exception& e) {
            std::cerr << "[FileSystemHandler] handleReadFile error: " << e.what() << "\n";

            json response = {
                    {"command", "fs.readFile.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void FileSystemHandler::handleWriteFile(const json& msg, WebSocketSession& session) {
        try {
            validateAuth(session);

            std::string mountName = msg.at("mountName").get<std::string>();
            std::string path = msg.at("path").get<std::string>();
            std::string data = msg.at("data").get<std::string>();

            bool success = false;

            try {
                auto fsManager = storageManager_->getLocalEngine(mountName);
                std::vector<uint8_t> binaryData(data.begin(), data.end());
                success = fsManager->writeFile(path, binaryData);
            } catch (...) {
                auto cloudProvider = storageManager_->getCloudEngine(mountName);
                // TODO: Fix cloud write logic, make better use of polymorphic classes
                // cloudProvider->writeFile(path, data);
                success = true;
            }

            json response = {
                    {"command", "fs.writeFile.response"},
                    {"status", success ? "ok" : "error"},
                    {"mountName", mountName},
                    {"path", path}
            };

            session.send(response);

            std::cout << "[FileSystemHandler] WriteFile on mount '" << mountName << "' path '" << path << "'\n";

        } catch (const std::exception& e) {
            std::cerr << "[FileSystemHandler] handleWriteFile error: " << e.what() << "\n";

            json response = {
                    {"command", "fs.writeFile.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void FileSystemHandler::handleDeleteFile(const json& msg, WebSocketSession& session) {
        try {
            validateAuth(session);

            std::string mountName = msg.at("mountName").get<std::string>();
            std::string path = msg.at("path").get<std::string>();

            bool success = false;

            try {
                auto fsManager = storageManager_->getLocalEngine(mountName);
                success = fsManager->deleteFile(path);
            } catch (...) {
                auto cloudProvider = storageManager_->getCloudEngine(mountName);
                cloudProvider->deleteFile(path);
                success = true;
            }

            json response = {
                    {"command", "fs.deleteFile.response"},
                    {"status", success ? "ok" : "error"},
                    {"mountName", mountName},
                    {"path", path}
            };

            session.send(response);

            std::cout << "[FileSystemHandler] DeleteFile on mount '" << mountName << "' path '" << path << "'\n";

        } catch (const std::exception& e) {
            std::cerr << "[FileSystemHandler] handleDeleteFile error: " << e.what() << "\n";

            json response = {
                    {"command", "fs.deleteFile.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

} // namespace vh::websocket
