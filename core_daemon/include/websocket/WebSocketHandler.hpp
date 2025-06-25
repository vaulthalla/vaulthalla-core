#pragma once

#include "services/ServiceManager.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/handlers/NotificationHandler.hpp"
#include "websocket/handlers/SearchHandler.hpp"
#include "websocket/handlers/ShareHandler.hpp"
#include "websocket/handlers/StorageHandler.hpp"
#include "websocket/handlers/PermissionsHandler.hpp"
#include <memory>

namespace vh::websocket {

class WebSocketHandler {
  public:
    explicit WebSocketHandler(const std::shared_ptr<vh::services::ServiceManager>& serviceManager,
                              const std::shared_ptr<WebSocketRouter>& router);

    void registerAllHandlers();

  private:
    std::shared_ptr<WebSocketRouter> router_;
    std::shared_ptr<vh::services::ServiceManager> serviceManager_;

    std::shared_ptr<AuthHandler> authHandler_;
    std::shared_ptr<FileSystemHandler> fsHandler_;
    std::shared_ptr<StorageHandler> storageHandler_;
    std::shared_ptr<PermissionsHandler> permissionsHandler_;
    std::shared_ptr<ShareHandler> shareHandler_;
    std::shared_ptr<SearchHandler> searchHandler_;
    std::shared_ptr<NotificationHandler> notificationHandler_;
};

} // namespace vh::websocket
