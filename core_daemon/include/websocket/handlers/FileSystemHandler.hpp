#pragma once

#include "services/ServiceManager.hpp"
#include "storage/StorageManager.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

class FileSystemHandler {
public:
    explicit FileSystemHandler(const std::shared_ptr<services::ServiceManager>& serviceManager);

    void handleUploadStart(const json& msg, WebSocketSession& session);

    void handleUploadFinish(const json& msg, WebSocketSession& session);

    void handleMkdir(const json& msg, WebSocketSession& session);

    void handleListDir(const json& msg, WebSocketSession& session);

    void handleReadFile(const json& msg, WebSocketSession& session);

    void handleDeleteFile(const json& msg, WebSocketSession& session);

private:
    std::shared_ptr<storage::StorageManager> storageManager_;

    template <typename... Funcs> static void enforcePermissions(
        WebSocketSession& session,
        const unsigned int vaultId,
        const std::filesystem::path& path, // Add path param
        Funcs... checks) {
        const auto user = session.getAuthenticatedUser();
        if (!user) throw std::runtime_error("Unauthorized");

        if (user->isAdmin()) return;

        const auto role = user->getRole(vaultId);
        if (!role) throw std::runtime_error("No role assigned for this vault/volume");

        if (!( ((role.get()->*checks)(path)) || ... )) {
            throw std::runtime_error("Permission denied: Required permission not granted");
        }
    }

};

} // namespace vh::websocket