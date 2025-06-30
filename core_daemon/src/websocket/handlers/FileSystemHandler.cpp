#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "types/db/File.hpp"
#include "types/db/AssignedRole.hpp"
#include "websocket/handlers/UploadHandler.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include <iostream>

namespace vh::websocket {

FileSystemHandler::FileSystemHandler(const std::shared_ptr<services::ServiceManager>& serviceManager)
    : storageManager_(serviceManager->storageManager()) {
    if (!storageManager_) throw std::invalid_argument("StorageManager cannot be null");
}

void FileSystemHandler::handleUploadStart(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto volumeId = payload.at("volume_id").get<unsigned int>();
        const auto path = payload.at("path").get<std::string>();

        enforcePermissions(session, vaultId, volumeId, &types::AssignedRole::canUploadFile);

        const auto uploadId = WebSocketSession::generateUUIDv4();
        const auto engine = storageManager_->getEngine(volumeId);
        if (!engine) throw std::runtime_error("Unknown storage engine");

        if (engine->type() != storage::StorageType::Local) {
            // TODO: Handle other storage types (e.g., S3, Azure, etc.)
            throw std::runtime_error("Unsupported storage engine type for upload, requires Local");
        }

        const auto localEngine = std::static_pointer_cast<storage::LocalDiskStorageEngine>(engine);

        const auto absPath = localEngine->getAbsolutePath(path, volumeId);
        const auto tmpPath = absPath.parent_path() / (".upload-" + uploadId + ".part");

        session.getUploadHandler()->startUpload(uploadId, tmpPath, absPath, payload.at("size").get<uint64_t>());

        const json data = {
            {"upload_id", uploadId}
        };

        const json response = {{"command", "fs.upload.start.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        std::cout << "[FileSystemHandler] UploadStart on vault '" << vaultId << "' path '" << path << "'"
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleUploadStart error: " << e.what() << std::endl;
        const json response = {{"command", "fs.upload.start.response"}, {"status", "error"}, {"error", e.what()}};
        session.send(response);
    }
}

void FileSystemHandler::handleUploadFinish(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto volumeId = payload.at("volume_id").get<unsigned int>();
        const auto path = payload.at("path").get<std::string>();

        enforcePermissions(session, vaultId, volumeId, &types::AssignedRole::canUploadFile);

        session.getUploadHandler()->finishUpload();

        const json data = {{"path", path}};

        const json response = {
            {"command", "fs.upload.finish.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };
        session.send(response);

        std::cout << "[FileSystemHandler] UploadFinish on vault '" << vaultId << "' path '" << path << "'" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleUploadFinish error: " << e.what() << std::endl;

        const json response = {
            {"command", "fs.upload.finish.response"},
            {"status", "error"},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void FileSystemHandler::handleListDir(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto volumeId = payload.at("volume_id").get<unsigned int>();
        const auto path = payload.value("path", "/");

        const auto user = session.getAuthenticatedUser();
        enforcePermissions(session, vaultId, volumeId, &types::AssignedRole::canListDirectory);

        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("Unknown storage engine: " + vaultId);

        const auto files = engine->listFilesInDir(volumeId, path, false);
        if (files.empty()) throw std::runtime_error("Directory not found or empty: " + path);

        const json data = {
            {"vault", engine->getVault()->name},
            {"path", path},
            {"files", files}
        };

        const json response = {{"command", "fs.dir.list.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}
        };

        session.send(response);

        std::cout << "[FileSystemHandler] ListDir on mount '" << engine->getVault()->name << "' path '" << path <<
            std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleListDir error: " << e.what() << std::endl;

        const json response = {{"command", "fs.listDir.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleReadFile(const json& msg, WebSocketSession& session) {
    try {
        std::string mountName = msg.at("mountName").get<std::string>();
        unsigned int volumeId = msg.at("volumeId").get<unsigned int>();
        std::string path = msg.at("path").get<std::string>();

        // TODO: Validate auth and permissions

        auto engine = storageManager_->getEngine(volumeId);
        if (!engine) throw std::runtime_error("Unknown storage engine: " + mountName);
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

void FileSystemHandler::handleDeleteFile(const json& msg, WebSocketSession& session) {
    try {
        std::string mountName = msg.at("mountName").get<std::string>();
        unsigned int volumeId = msg.at("volumeId").get<unsigned int>();
        std::string path = msg.at("path").get<std::string>();

        // TODO: Validate auth and permissions

        bool success = false;

        try {
            success = storageManager_->getLocalEngine(volumeId)->deleteFile(path);
        } catch (...) {
            storageManager_->getCloudEngine(volumeId)->deleteFile(path);
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