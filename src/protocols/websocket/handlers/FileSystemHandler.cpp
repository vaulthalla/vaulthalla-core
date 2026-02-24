#include "protocols/websocket/handlers/FileSystemHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "storage/Manager.hpp"
#include "fs/model/File.hpp"
#include "fs/model/Path.hpp"
#include "rbac/model/VaultRole.hpp"
#include "vault/model/Vault.hpp"
#include "identities/model/User.hpp"
#include "protocols/websocket/handlers/UploadHandler.hpp"
#include "storage/Engine.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"
#include "fs/Filesystem.hpp"
#include "services/SyncController.hpp"
#include "fs/cache/Registry.hpp"

using namespace vh::vault::model;
using namespace vh::rbac::model;
using namespace vh::services;
using namespace vh::storage;
using namespace vh::logging;
using namespace vh::fs;
using namespace vh::fs::model;

namespace vh::websocket {

json FileSystemHandler::startUpload(const json& payload, WebSocketSession& session) {
    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    const auto path = payload.at("path").get<std::string>();

    enforcePermissions(session, vaultId, path, &VaultRole::canCreate);

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("Unknown storage engine");

    const auto uploadId = WebSocketSession::generateUUIDv4();
    const auto absPath = engine->paths->absPath(path, PathType::VAULT_ROOT);
    const auto tmpPath = absPath.parent_path() / (".upload-" + uploadId + ".part");

    session.getUploadHandler()->startUpload( {
        .uploadId = uploadId,
        .expectedSize = payload.at("size").get<uint64_t>(),
        .engine = engine,
        .tmpPath = tmpPath,
        .finalPath = absPath,
        .fuseFrom = engine->paths->absRelToRoot(tmpPath, PathType::FUSE_ROOT),
        .fuseTo = engine->paths->absRelToRoot(absPath, PathType::FUSE_ROOT)
    });

    return {{"upload_id", uploadId}};
}

json FileSystemHandler::finishUpload(const json& payload, WebSocketSession& session) {
    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    const auto path = payload.at("path").get<std::string>();
    enforcePermissions(session, vaultId, path, &VaultRole::canCreate);
    session.getUploadHandler()->finishUpload();
    return {{"path", path}};
}

json FileSystemHandler::mkdir(const json& payload, WebSocketSession& session) {
    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    const auto path = payload.at("path").get<std::string>();

    enforcePermissions(session, vaultId, path, &VaultRole::canCreate);

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->mkdir(path, session.getAuthenticatedUser()->id);
    ServiceDepsRegistry::instance().syncController->runNow(vaultId);

    return {{"path", path}};
}

json FileSystemHandler::move(const json& payload, WebSocketSession& session) {
    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    const auto from = payload.at("from").get<std::string>();
    const auto to = payload.at("to").get<std::string>();

    enforcePermissions(session, vaultId, from, &VaultRole::canMove);
    enforcePermissions(session, vaultId, to, &VaultRole::canCreate);

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->move(from, to, session.getAuthenticatedUser()->id);
    ServiceDepsRegistry::instance().syncController->runNow(vaultId);

    return {{"from", from}, {"to", to}};
}

json FileSystemHandler::rename(const json& payload, WebSocketSession& session) {
    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    const auto from = std::filesystem::path(payload.at("from").get<std::string>());
    const auto to = std::filesystem::path(payload.at("to").get<std::string>());

    enforcePermissions(session, vaultId, from, &VaultRole::canRename);
    enforcePermissions(session, vaultId, to, &VaultRole::canCreate);

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->rename(from, to, session.getAuthenticatedUser()->id);
    ServiceDepsRegistry::instance().syncController->runNow(vaultId);

    return {{"from", from}, {"to", to}};
}

json FileSystemHandler::copy(const json& payload, WebSocketSession& session) {
    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    const auto from = std::filesystem::path(payload.at("from").get<std::string>());
    const auto to = std::filesystem::path(payload.at("to").get<std::string>());

    enforcePermissions(session, vaultId, from, &VaultRole::canMove);
    enforcePermissions(session, vaultId, to, &VaultRole::canCreate);

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->copy(from, to, session.getAuthenticatedUser()->id);
    ServiceDepsRegistry::instance().syncController->runNow(vaultId);

    return {{"from", from}, {"to", to}};
}

json FileSystemHandler::listDir(const json& payload, WebSocketSession& session) {
    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    auto path = std::filesystem::path(payload.value("path", "/"));
    if (path.empty()) path = std::filesystem::path("/");

    enforcePermissions(session, vaultId, path, &VaultRole::canList);

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));

    const auto fusePath = engine->paths->absRelToAbsRel(path, PathType::VAULT_ROOT, PathType::FUSE_ROOT);
    if (!Filesystem::exists(fusePath)) throw std::runtime_error("Path does not exist: " + fusePath.string());

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(fusePath);
    if (!entry) throw std::runtime_error("No entry found for path: " + fusePath.string());

    const auto entries = ServiceDepsRegistry::instance().fsCache->listDir(entry->id, false);

    return {
                {"vault", engine->vault->name},
                {"path", path},
                {"files", entries}
    };
}

json FileSystemHandler::remove(const json& payload, WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    const auto userId = user->id;
    if (userId == 0) throw std::runtime_error("User not authenticated");

    const auto vaultId = payload.at("vault_id").get<unsigned int>();
    const auto path = std::filesystem::path(payload.at("path").get<std::string>());

    enforcePermissions(session, vaultId, path, &VaultRole::canDelete);

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->remove(path, userId);
    ServiceDepsRegistry::instance().syncController->runNow(vaultId);

    return {{"path", path.string()}};
}

} // namespace vh::websocket