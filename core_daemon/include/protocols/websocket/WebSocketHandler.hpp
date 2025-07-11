#pragma once

#include "services/ServiceManager.hpp"
#include "protocols/websocket/WebSocketRouter.hpp"
#include "protocols/websocket/handlers/AuthHandler.hpp"
#include "protocols/websocket/handlers/FileSystemHandler.hpp"
#include "protocols/websocket/handlers/NotificationHandler.hpp"
#include "protocols/websocket/handlers/SearchHandler.hpp"
#include "protocols/websocket/handlers/ShareHandler.hpp"
#include "protocols/websocket/handlers/StorageHandler.hpp"
#include <memory>

namespace vh::websocket {

class WebSocketHandler {
  public:
    explicit WebSocketHandler(const std::shared_ptr<services::ServiceManager>& serviceManager,
                              const std::shared_ptr<WebSocketRouter>& router);

    void registerAllHandlers() const;

  private:
    std::shared_ptr<WebSocketRouter> router_;
    std::shared_ptr<services::ServiceManager> serviceManager_;

    std::shared_ptr<AuthHandler> authHandler_;
    std::shared_ptr<FileSystemHandler> fsHandler_;
    std::shared_ptr<StorageHandler> storageHandler_;
    std::shared_ptr<ShareHandler> shareHandler_;
    std::shared_ptr<SearchHandler> searchHandler_;
    std::shared_ptr<NotificationHandler> notificationHandler_;

    void registerAuthHandlers() const;
    void registerFileSystemHandlers() const;
    void registerStorageHandlers() const;
    void registerAPIKeyHandlers() const;
    void registerPermissionsHandlers() const;
    void registerSettingsHandlers() const;
    void registerGroupHandlers() const;
    void registerShareHandlers() const;
    void registerSearchHandlers() const;
    void registerNotificationHandlers() const;

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
