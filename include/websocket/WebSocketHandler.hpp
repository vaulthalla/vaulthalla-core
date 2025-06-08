#pragma once

#include <memory>

namespace vh::websocket {
    namespace vh::auth {
        class SessionManager;
    }

    namespace vh::core {
        class FSManager;
    }

    namespace vh::index {
        class SearchIndex;
    }

    class WebSocketRouter;
    class AuthHandler;
    class FileSystemHandler;
    class StorageHandler;
    class ShareHandler;
    class SearchHandler;
    class NotificationHandler;

    class WebSocketHandler {
    public:
        WebSocketHandler(WebSocketRouter& router,
                         vh::auth::SessionManager& sessionManager,
                         std::shared_ptr<vh::core::FSManager> fsManager,
                         std::shared_ptr<vh::index::SearchIndex> searchIndex);

        void registerAllHandlers();

    private:
        WebSocketRouter& router_;
        vh::auth::SessionManager& sessionManager_;
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
