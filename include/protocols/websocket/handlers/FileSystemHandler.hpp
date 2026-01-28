#pragma once

#include "protocols/websocket/WebSocketSession.hpp"
#include "types/User.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class FileSystemHandler {
public:
    static json startUpload(const json& payload, WebSocketSession& session);

    static json finishUpload(const json& payload, WebSocketSession& session);

    static json mkdir(const json& payload, WebSocketSession& session);

    static json move(const json& payload, WebSocketSession& session);

    static json rename(const json& payload, WebSocketSession& session);

    static json copy(const json& payload, WebSocketSession& session);

    static json listDir(const json& payload, WebSocketSession& session);

    static json remove(const json& payload, WebSocketSession& session);

private:
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