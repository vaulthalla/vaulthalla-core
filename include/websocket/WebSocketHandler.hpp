#pragma once

#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/handlers/StorageHandler.hpp"
#include "websocket/handlers/ShareHandler.hpp"
#include "websocket/handlers/SearchHandler.hpp"
#include "websocket/handlers/NotificationHandler.hpp"
#include "services/ServiceManager.hpp"

#include <memory>

namespace vh::websocket {

    class WebSocketHandler {
    public:
        WebSocketHandler(WebSocketRouter& router, const std::shared_ptr<vh::services::ServiceManager>& serviceManager);

        void registerAllHandlers();

    private:
        WebSocketRouter& router_;
        std::shared_ptr<vh::services::ServiceManager> serviceManager_;

        std::shared_ptr<AuthHandler> authHandler_;
        std::shared_ptr<FileSystemHandler> fsHandler_;
        std::shared_ptr<StorageHandler> storageHandler_;
        std::shared_ptr<ShareHandler> shareHandler_;
        std::shared_ptr<SearchHandler> searchHandler_;
        std::shared_ptr<NotificationHandler> notificationHandler_;
    };

} // namespace vh::websocket
