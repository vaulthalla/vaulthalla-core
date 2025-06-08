#pragma once

#include "websocket/WebSocketRouter.hpp"
#include "auth/SessionManager.hpp"
#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/handlers/StorageHandler.hpp"
// Future:
#include "websocket/handlers/ShareHandler.hpp"
#include "websocket/handlers/SearchHandler.hpp"
#include "websocket/handlers/NotificationHandler.hpp"

#include <memory>

namespace vh::websocket {

    class WebSocketHandler {
    public:
        WebSocketHandler(WebSocketRouter& router,
                         auth::SessionManager& sessionManager,
                         std::shared_ptr<vh::core::FSManager> fsManager,
                         std::shared_ptr<vh::index::SearchIndex> searchIndex);

        void registerAllHandlers();

    private:
        WebSocketRouter& router_;
        auth::SessionManager& sessionManager_;
        std::shared_ptr<vh::core::FSManager> fsManager_;
        std::shared_ptr<vh::index::SearchIndex> searchIndex_;

        std::shared_ptr<AuthHandler> authHandler_;
        std::shared_ptr<FileSystemHandler> fsHandler_;
        std::shared_ptr<StorageHandler> storageHandler_;
        std::shared_ptr<ShareHandler> shareHandler_;
        std::shared_ptr<SearchHandler> searchHandler_;
        std::shared_ptr<NotificationHandler> notificationHandler_;
    };

} // namespace vh::websocket
