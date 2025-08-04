#include "protocols/websocket/handlers/FileSystemHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "types/File.hpp"
#include "types/VaultRole.hpp"
#include "types/Vault.hpp"
#include "types/Path.hpp"
#include "protocols/websocket/handlers/UploadHandler.hpp"
#include "storage/StorageEngine.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::types;
using namespace vh::services;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::logging;

namespace vh::websocket {

FileSystemHandler::FileSystemHandler()
    : storageManager_(ServiceDepsRegistry::instance().storageManager) {
    if (!storageManager_) throw std::invalid_argument("StorageManager cannot be null");
}

void FileSystemHandler::handleUploadStart(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = payload.at("path").get<std::string>();

        enforcePermissions(session, vaultId, path, &VaultRole::canCreate);

        const auto uploadId = WebSocketSession::generateUUIDv4();
        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("Unknown storage engine");

        const auto absPath = engine->paths->absPath(path, PathType::VAULT_ROOT);
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

        LogRegistry::fs()->info("[FileSystemHandler] UploadStart on vault '{}' path '{}' with upload ID '{}'",
                               vaultId, path, uploadId);

    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleUploadStart error: {}", e.what());
        const json response = {{"command", "fs.upload.start.response"}, {"status", "error"}, {"error", e.what()}};
        session.send(response);
    }
}

void FileSystemHandler::handleUploadFinish(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = payload.at("path").get<std::string>();

        enforcePermissions(session, vaultId, path, &VaultRole::canCreate);

        session.getUploadHandler()->finishUpload(storageManager_->getEngine(vaultId));

        const json data = {{"path", path}};

        const json response = {
            {"command", "fs.upload.finish.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };
        session.send(response);

        LogRegistry::fs()->info("[FileSystemHandler] UploadFinish on vault '{}' path '{}'", vaultId, path);

    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleUploadFinish error: {}", e.what());

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

        enforcePermissions(session, vaultId, path, &VaultRole::canCreate);

        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
        engine->mkdir(path, session.getAuthenticatedUser()->id);
        ServiceDepsRegistry::instance().syncController->runNow(vaultId);

        const json data = {{"path", path}};

        const json response = {{"command", "fs.dir.create.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        LogRegistry::fs()->info("[FileSystemHandler] Mkdir on vault '{}' path '{}'", vaultId, path);

    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleMkdir error: {}", e.what());

        const json response = {{"command", "fs.dir.create.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleMove(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto from = payload.at("from").get<std::string>();
        const auto to = payload.at("to").get<std::string>();

        enforcePermissions(session, vaultId, from, &VaultRole::canMove);
        enforcePermissions(session, vaultId, to, &VaultRole::canCreate);

        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
        engine->move(from, to, session.getAuthenticatedUser()->id);
        ServiceDepsRegistry::instance().syncController->runNow(vaultId);

        const json data = {{"from", from}, {"to", to}};

        const json response = {{"command", "fs.entry.move.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        LogRegistry::fs()->info("[FileSystemHandler] Move on vault '{}' from '{}' to '{}'", vaultId, from, to);

    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleMove error: {}", e.what());

        const json response = {{"command", "fs.entry.move.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleRename(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto from = fs::path(payload.at("from").get<std::string>());
        const auto to = fs::path(payload.at("to").get<std::string>());

        enforcePermissions(session, vaultId, from, &VaultRole::canRename);
        enforcePermissions(session, vaultId, to, &VaultRole::canCreate);

        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
        engine->rename(from, to, session.getAuthenticatedUser()->id);
        ServiceDepsRegistry::instance().syncController->runNow(vaultId);

        const json data = {{"from", from}, {"to", to}};

        const json response = {{"command", "fs.entry.rename.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        LogRegistry::fs()->info("[FileSystemHandler] Rename on vault '{}' from '{}' to '{}'", vaultId, from.string(), to.string());

    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleRename error: {}", e.what());

        const json response = {{"command", "fs.entry.rename.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleCopy(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto from = fs::path(payload.at("from").get<std::string>());
        const auto to = fs::path(payload.at("to").get<std::string>());

        enforcePermissions(session, vaultId, from, &VaultRole::canMove);
        enforcePermissions(session, vaultId, to, &VaultRole::canCreate);

        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
        engine->copy(from, to, session.getAuthenticatedUser()->id);
        ServiceDepsRegistry::instance().syncController->runNow(vaultId);

        const json data = {{"from", from}, {"to", to}};

        const json response = {{"command", "fs.entry.copy.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};

        session.send(response);

        LogRegistry::fs()->info("[FileSystemHandler] Copy on vault '{}' from '{}' to '{}'", vaultId, from.string(), to.string());

    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleCopy error: {}", e.what());

        const json response = {{"command", "fs.entry.copy.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleListDir(const json& msg, WebSocketSession& session) {
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        auto path = fs::path(payload.value("path", "/"));
        if (path.empty()) path = fs::path("/");

        enforcePermissions(session, vaultId, path, &VaultRole::canList);

        const auto& vaultName = storageManager_->getVault(vaultId)->name;

        const json data = {
            {"vault", vaultName},
            {"path", path},
            {"files", DirectoryQueries::listDir(vaultId, path, false)}
        };

        const json response = {{"command", "fs.dir.list.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}
        };

        LogRegistry::fs()->debug("[FileSystemHandler] handleListDir on vault '{}' path '{}'", vaultId, path.string());

        session.send(response);

    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleListDir error: {}", e.what());

        const json response = {{"command", "fs.listDir.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void FileSystemHandler::handleDelete(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user) throw std::runtime_error("User not authenticated");

        const auto userId = user->id;
        if (userId == 0) throw std::runtime_error("User not authenticated");

        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id").get<unsigned int>();
        const auto path = std::filesystem::path(payload.at("path").get<std::string>());

        enforcePermissions(session, vaultId, path, &VaultRole::canDelete);

        const auto engine = storageManager_->getEngine(vaultId);
        if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
        engine->remove(path, userId);
        ServiceDepsRegistry::instance().syncController->runNow(vaultId);

        const json response = {{"command", "fs.entry.delete.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", {{"path", path.string()}}}};

        session.send(response);

        LogRegistry::fs()->info("[FileSystemHandler] Delete on vault '{}' path '{}'", vaultId, path.string());
    } catch (const std::exception& e) {
        LogRegistry::fs()->error("[FileSystemHandler] handleDelete error: {}", e.what());

        const json response = {{"command", "fs.entry.delete.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

} // namespace vh::websocket