#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "security/PermissionManager.hpp"
#include "storage/StorageManager.hpp"
#include "auth/User.hpp"
#include <iostream>

namespace vh::websocket {

    FileSystemHandler::FileSystemHandler(std::shared_ptr<vh::storage::StorageManager> storageManager)
            : storageManager_(std::move(storageManager)) {}

    FileSystemHandler::FileSystemHandler(std::shared_ptr<vh::storage::StorageManager> storageManager,
                                         std::shared_ptr<vh::security::PermissionManager> permissionManager)
            : storageManager_(std::move(storageManager)),
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

            auto engine = storageManager_->getEngine(mountName);
            if (!engine) throw std::runtime_error("Unknown storage engine: " + mountName);
            enforcePermission(session, mountName, path, "list");
            auto files = engine->listFilesInDir(path);
            json entries = json::array();
            if (files.empty()) throw std::runtime_error("Directory not found or empty: " + path);
            for (const auto& file : files) entries.push_back(file.string());

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

            auto engine = storageManager_->getEngine(mountName);
            if (!engine) throw std::runtime_error("Unknown storage engine: " + mountName);
            enforcePermission(session, mountName, path, "read");
            auto data = engine->readFile(path);
            if (!data.has_value()) throw std::runtime_error("File not found: " + path);
            auto fileContent = std::string(data->begin(), data->end());

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

            auto engine = storageManager_->getEngine(mountName);
            if (!engine) throw std::runtime_error("Unknown storage engine: " + mountName);
            std::vector<uint8_t> binaryData(data.begin(), data.end());
            enforcePermission(session, mountName, path, "write");
            bool success = engine->writeFile(path, binaryData);

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
