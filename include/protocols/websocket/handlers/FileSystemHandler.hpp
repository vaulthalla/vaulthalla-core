#pragma once

#include "protocols/websocket/WebSocketSession.hpp"
#include "storage/StorageManager.hpp"
#include "types/User.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class FileSystemHandler {
public:
    FileSystemHandler();

    void handleUploadStart(const json& msg, WebSocketSession& session);

    void handleUploadFinish(const json& msg, WebSocketSession& session);

    void handleMkdir(const json& msg, WebSocketSession& session);

    void handleMove(const json& msg, WebSocketSession& session);

    void handleRename(const json& msg, WebSocketSession& session);

    void handleCopy(const json& msg, WebSocketSession& session);

    void handleListDir(const json& msg, WebSocketSession& session);

    void handleDelete(const json& msg, WebSocketSession& session);

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