#pragma once

#include "services/ServiceManager.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/handlers/NotificationHandler.hpp"
#include "websocket/handlers/SearchHandler.hpp"
#include "websocket/handlers/ShareHandler.hpp"
#include "websocket/handlers/StorageHandler.hpp"
#include <memory>

namespace vh::websocket {

class WebSocketHandler {
  public:
    explicit WebSocketHandler(const std::shared_ptr<vh::services::ServiceManager>& serviceManager,
                              const std::shared_ptr<WebSocketRouter>& router);

    void registerAllHandlers() const;

  private:
    std::shared_ptr<WebSocketRouter> router_;
    std::shared_ptr<vh::services::ServiceManager> serviceManager_;

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
