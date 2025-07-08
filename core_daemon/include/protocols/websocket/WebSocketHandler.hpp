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
};

} // namespace vh::websocket
