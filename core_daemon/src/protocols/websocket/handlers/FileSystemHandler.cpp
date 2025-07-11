#include "protocols/websocket/handlers/FileSystemHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "types/File.hpp"
#include "types/VaultRole.hpp"
#include "protocols/websocket/handlers/UploadHandler.hpp"
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
        const auto path = payload.at("path").get<std::string>();

        enforcePermissions(session, vaultId, path, &types::VaultRole::canCreate);

        const auto uploadId = WebSocketSession::generateUUIDv4();
        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("Unknown storage engine");

        const auto absPath = engine->getAbsolutePath(path);
        const auto tmpPath = absPath.parent_path() / (".upload-" + uploadId + ".part");

        UploadHandler::ensureDirectoriesInDb(vaultId, path, session.getAuthenticatedUser());

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
        const auto path = payload.at("path").get<std::string>();

        enforcePermissions(session, vaultId, path, &types::VaultRole::canCreate);

        session.getUploadHandler()->finishUpload();

        UploadContext context;

        storageManager_->finishUpload(vaultId, path, session.getAuthenticatedUser());

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

void FileSystemHandler::handleMkdir(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = payload.at("path").get<std::string>();

        enforcePermissions(session, vaultId, path, &types::VaultRole::canCreate);

        storageManager_->mkdir(vaultId, path, session.getAuthenticatedUser());

        const json data = {{"path", path}};

        const json response = {{"command", "fs.dir.create.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        std::cout << "[FileSystemHandler] Mkdir on vault '" << vaultId << "' path '" << path << "'" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleMkdir error: " << e.what() << std::endl;

        const json response = {{"command", "fs.dir.create.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleListDir(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = payload.value("path", "/");

        const auto user = session.getAuthenticatedUser();
        enforcePermissions(session, vaultId, path, &types::VaultRole::canList);

        const auto& vaultName = storageManager_->getVault(vaultId)->name;
        const auto files = storageManager_->listDir(vaultId, path);

        const json data = {
            {"vault", vaultName},
            {"path", path},
            {"files", files}
        };

        const json response = {{"command", "fs.dir.list.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}
        };

        session.send(response);

        std::cout << "[FileSystemHandler] ListDir on mount '" << vaultName << "' path '" << path <<
            std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleListDir error: " << e.what() << std::endl;

        const json response = {{"command", "fs.listDir.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleReadFile(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto mountName = payload.at("mountName").get<std::string>();
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = payload.at("path").get<std::string>();

        // TODO: Validate auth and permissions

        const auto engine = storageManager_->getEngine(vaultId);
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

void FileSystemHandler::handleDelete(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = payload.at("path").get<std::string>();

        // TODO: Validate auth and permissions

        storageManager_->removeEntry(vaultId, path);

        const json response = {{"command", "fs.entry.delete.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", {{"path", path}}}};

        session.send(response);

        std::cout << "[FileSystemHandler] DeleteFile on mount '" << vaultId << "' path '" << path << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleDeleteFile error: " << e.what() << std::endl;

        const json response = {{"command", "fs.entry.delete.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

} // namespace vh::websocket