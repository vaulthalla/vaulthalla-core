#pragma once

#include "protocols/websocket/WebSocketSession.hpp"
#include "types/User.hpp"

#include <memory>

namespace vh::websocket {

class WebSocketRouter;

class WebSocketHandler {
  public:
    explicit WebSocketHandler(const std::shared_ptr<WebSocketRouter>& router);

    void registerAllHandlers() const;

  private:
    std::shared_ptr<WebSocketRouter> router_;

    void registerAuthHandlers() const;
    void registerFileSystemHandlers() const;
    void registerStorageHandlers() const;
    void registerAPIKeyHandlers() const;
    void registerRoleHandlers() const;
    void registerPermissionsHandlers() const;
    void registerSettingsHandlers() const;
    void registerGroupHandlers() const;
    void registerStatHandlers() const;

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
