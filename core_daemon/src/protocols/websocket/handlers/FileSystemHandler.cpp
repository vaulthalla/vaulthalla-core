#include "protocols/websocket/handlers/FileSystemHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "types/File.hpp"
#include "types/VaultRole.hpp"
#include "types/Vault.hpp"
#include "protocols/websocket/handlers/UploadHandler.hpp"
#include "storage/StorageEngine.hpp"
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

        storageManager_->finishUpload(vaultId, session.getAuthenticatedUser()->id, path);

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

void FileSystemHandler::handleMove(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto fromPath = payload.at("from").get<std::string>();
        const auto toPath = payload.at("to").get<std::string>();

        enforcePermissions(session, vaultId, fromPath, &types::VaultRole::canMove);
        enforcePermissions(session, vaultId, toPath, &types::VaultRole::canCreate);

        storageManager_->move(vaultId, session.getAuthenticatedUser()->id, fromPath, toPath);

        const json data = {{"from", fromPath}, {"to", toPath}};

        const json response = {{"command", "fs.entry.move.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        std::cout << "[FileSystemHandler] Move on vault '" << vaultId << "' from '" << fromPath << "' to '" << toPath << "'" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleMove error: " << e.what() << std::endl;

        const json response = {{"command", "fs.entry.move.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleRename(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto fromPath = payload.at("from").get<std::string>();
        const auto toPath = payload.at("to").get<std::string>();

        enforcePermissions(session, vaultId, fromPath, &types::VaultRole::canRename);
        enforcePermissions(session, vaultId, toPath, &types::VaultRole::canCreate);

        storageManager_->rename(vaultId, session.getAuthenticatedUser()->id, fromPath, toPath);

        const json data = {{"from", fromPath}, {"to", toPath}};

        const json response = {{"command", "fs.entry.rename.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        std::cout << "[FileSystemHandler] Rename on vault '" << vaultId << "' from '" << fromPath << "' to '" << toPath << "'" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleRename error: " << e.what() << std::endl;

        const json response = {{"command", "fs.entry.rename.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleCopy(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto fromPath = payload.at("from").get<std::string>();
        const auto toPath = payload.at("to").get<std::string>();

        enforcePermissions(session, vaultId, fromPath, &types::VaultRole::canMove);
        enforcePermissions(session, vaultId, toPath, &types::VaultRole::canCreate);

        storageManager_->copy(vaultId, session.getAuthenticatedUser()->id, fromPath, toPath);

        const json data = {{"from", fromPath}, {"to", toPath}};

        const json response = {{"command", "fs.entry.copy.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        std::cout << "[FileSystemHandler] Copy on vault '" << vaultId << "' from '" << fromPath << "' to '" << toPath << "'" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleCopy error: " << e.what() << std::endl;

        const json response = {{"command", "fs.entry.copy.response"}, {"status", "error"}, {"error", e.what()}};

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

void FileSystemHandler::handleDelete(const json& msg, WebSocketSession& session) {
    try {
        const auto userId = session.getAuthenticatedUser()->id;
        if (userId == 0) throw std::runtime_error("User not authenticated");

        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = std::filesystem::path(payload.at("path").get<std::string>());

        enforcePermissions(session, vaultId, path, &types::VaultRole::canDelete);
        storageManager_->removeEntry(userId, vaultId, path);

        const json response = {{"command", "fs.entry.delete.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", {{"path", path.string()}}}};

        session.send(response);
    } catch (const std::exception& e) {
        std::cerr << "[FileSystemHandler] handleDeleteFile error: " << e.what() << std::endl;

        const json response = {{"command", "fs.entry.delete.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

} // namespace vh::websocket