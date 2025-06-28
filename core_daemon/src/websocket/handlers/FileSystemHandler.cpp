#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "types/db/File.hpp"
#include <iostream>

namespace vh::websocket {

FileSystemHandler::FileSystemHandler(const std::shared_ptr<services::ServiceManager>& serviceManager)
    : storageManager_(serviceManager->storageManager()) {
    if (!storageManager_) throw std::invalid_argument("StorageManager cannot be null");
}

void FileSystemHandler::validateAuth(WebSocketSession& session) {
    if (!session.getAuthenticatedUser()) {
        throw std::runtime_error("Unauthorized");
    }
}

void FileSystemHandler::enforcePermission(WebSocketSession& session, const std::string& mountName,
                                          const std::string& path, const std::string& requiredPermission) {
    auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("Unauthorized");
}

void FileSystemHandler::handleListDir(const json& msg, WebSocketSession& session) {
    try {
        validateAuth(session);

        const auto payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto volumeId = payload.at("volume_id").get<unsigned int>();
        const auto path = payload.value("path", "/");

        const auto engine = storageManager_->getEngine(volumeId);
        if (!engine) throw std::runtime_error("Unknown storage engine: " + vaultId);
        const auto files = engine->listFilesInDir(path, false);
        if (files.empty()) throw std::runtime_error("Directory not found or empty: " + path);

        const json data = {
            {"vault", engine->getVault()->name},
            {"path", path},
            {"entries", json::array()}
        };

        const json response = {{"command", "fs.dir.list.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}
        };

        session.send(response);

        std::cout << "[FileSystemHandler] ListDir on mount '" << engine->getVault()->name << "' path '" << path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleListDir error: " << e.what() << std::endl;

        const json response = {{"command", "fs.listDir.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleReadFile(const json& msg, WebSocketSession& session) {
    try {
        validateAuth(session);

        std::string mountName = msg.at("mountName").get<std::string>();
        unsigned int volumeId = msg.at("volumeId").get<unsigned int>();
        std::string path = msg.at("path").get<std::string>();

        auto engine = storageManager_->getEngine(volumeId);
        if (!engine) throw std::runtime_error("Unknown storage engine: " + mountName);
        enforcePermission(session, mountName, path, "read");
        auto data = engine->readFile(path);
        if (!data.has_value()) throw std::runtime_error("File not found: " + path);
        auto fileContent = std::string(data->begin(), data->end());

        json response = {{"command", "fs.readFile.response"},
                         {"status", "ok"},
                         {"mountName", mountName},
                         {"path", path},
                         {"data", fileContent}};

        session.send(response);

        std::cout << "[FileSystemHandler] ReadFile on mount '" << mountName << "' path '" << path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleReadFile error: " << e.what() << std::endl;

        json response = {{"command", "fs.readFile.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleWriteFile(const json& msg, WebSocketSession& session) {
    try {
        validateAuth(session);

        std::string mountName = msg.at("mountName").get<std::string>();
        unsigned int volumeId = msg.at("volumeId").get<unsigned int>();
        std::string path = msg.at("path").get<std::string>();
        std::string data = msg.at("data").get<std::string>();

        auto engine = storageManager_->getEngine(volumeId);
        if (!engine) throw std::runtime_error("Unknown storage engine: " + mountName);
        std::vector<uint8_t> binaryData(data.begin(), data.end());
        enforcePermission(session, mountName, path, "write");
        bool success = engine->writeFile(path, binaryData, false);

        json response = {{"command", "fs.writeFile.response"},
                         {"status", success ? "ok" : "error"},
                         {"mountName", mountName},
                         {"path", path}};

        session.send(response);

        std::cout << "[FileSystemHandler] WriteFile on mount '" << mountName << "' path '" << path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleWriteFile error: " << e.what() << std::endl;

        json response = {{"command", "fs.writeFile.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleDeleteFile(const json& msg, WebSocketSession& session) {
    try {
        validateAuth(session);

        std::string mountName = msg.at("mountName").get<std::string>();
        unsigned int volumeId = msg.at("volumeId").get<unsigned int>();
        std::string path = msg.at("path").get<std::string>();

        bool success = false;

        try {
            auto fsManager = storageManager_->getLocalEngine(volumeId);
            success = fsManager->deleteFile(path);
        } catch (...) {
            auto cloudProvider = storageManager_->getCloudEngine(volumeId);
            cloudProvider->deleteFile(path);
            success = true;
        }

        json response = {{"command", "fs.deleteFile.response"},
                         {"status", success ? "ok" : "error"},
                         {"mountName", mountName},
                         {"path", path}};

        session.send(response);

        std::cout << "[FileSystemHandler] DeleteFile on mount '" << mountName << "' path '" << path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleDeleteFile error: " << e.what() << std::endl;

        json response = {{"command", "fs.deleteFile.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

} // namespace vh::websocket