#pragma once

#include <memory>

namespace vh::auth {
    class SessionManager;
    class AuthManager;
    class TokenValidator;
}

namespace vh::core {
    class FSManager;
}

namespace vh::index {
    class SearchIndex;
}

namespace vh::security {
    class PermissionManager;
}

namespace vh::share {
    class LinkResolver;
}

namespace vh::websocket {
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
                         vh::auth::AuthManager& authManager,
                         vh::auth::TokenValidator& tokenValidator,
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

        std::shared_ptr<vh::security::PermissionManager> permissionManager_;
        std::shared_ptr<vh::share::LinkResolver> linkResolver_;

    };

} // namespace vh::websocket
