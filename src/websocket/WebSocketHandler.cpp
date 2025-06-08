#include "websocket/WebSocketHandler.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "auth/SessionManager.hpp"
#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/handlers/FileSystemHandler.hpp"
#include "websocket/handlers/StorageHandler.hpp"
// Future:
#include "websocket/handlers/ShareHandler.hpp"
#include "websocket/handlers/SearchHandler.hpp"
#include "websocket/handlers/NotificationHandler.hpp"

namespace vh::websocket {

    WebSocketHandler::WebSocketHandler(WebSocketRouter& router,
                                       auth::SessionManager& sessionManager,
                                       std::shared_ptr<vh::core::FSManager> fsManager,
                                       std::shared_ptr<vh::index::SearchIndex> searchIndex)
            : router_(router),
              sessionManager_(sessionManager),
              fsManager_(std::move(fsManager)),
              searchIndex_(std::move(searchIndex)) {
        // Construct handlers with deps
        authHandler_ = std::make_shared<AuthHandler>(sessionManager_);
        fsHandler_ = std::make_shared<FileSystemHandler>(fsManager_);
        storageHandler_ = std::make_shared<StorageHandler>();
        shareHandler_ = std::make_shared<ShareHandler>();
        searchHandler_ = std::make_shared<SearchHandler>(searchIndex_);
        notificationHandler_ = std::make_shared<NotificationHandler>();
    }

    void WebSocketHandler::registerAllHandlers() {
        // Auth
        router_.registerHandler("auth.login", [this](const json& msg, WebSocketSession& session) {
            authHandler_->handleLogin(msg, session);
        });

        router_.registerHandler("auth.refresh", [this](const json& msg, WebSocketSession& session) {
            authHandler_->handleRefresh(msg, session);
        });

        router_.registerHandler("auth.logout", [this](const json& msg, WebSocketSession& session) {
            authHandler_->handleLogout(msg, session);
        });

        // FileSystem
        router_.registerHandler("fs.listDir", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleListDir(msg, session);
        });

        router_.registerHandler("fs.readFile", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleReadFile(msg, session);
        });

        router_.registerHandler("fs.writeFile", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleWriteFile(msg, session);
        });

        router_.registerHandler("fs.deleteFile", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleDeleteFile(msg, session);
        });

        // Storage
        router_.registerHandler("storage.local.init", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleInitLocal(msg, session);
        });

        router_.registerHandler("storage.s3.init", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleInitS3(msg, session);
        });

        router_.registerHandler("storage.r2.init", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleInitR2(msg, session);
        });

        router_.registerHandler("storage.mount", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleMount(msg, session);
        });

        router_.registerHandler("storage.unmount", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleUnmount(msg, session);
        });

        // Share
        router_.registerHandler("share.createLink", [this](const json& msg, WebSocketSession& session) {
            shareHandler_->handleCreateLink(msg, session);
        });

        router_.registerHandler("share.resolveLink", [this](const json& msg, WebSocketSession& session) {
            shareHandler_->handleResolveLink(msg, session);
        });

        // Search
        router_.registerHandler("index.search", [this](const json& msg, WebSocketSession& session) {
            searchHandler_->handleSearch(msg, session);
        });

        // Notifications
        router_.registerHandler("notification.subscribe", [this](const json& msg, WebSocketSession& session) {
            notificationHandler_->handleSubscribe(msg, session);
        });

        router_.registerHandler("notification.unsubscribe", [this](const json& msg, WebSocketSession& session) {
            notificationHandler_->handleUnsubscribe(msg, session);
        });

        std::cout << "[WebSocketHandler] All handlers registered.\n";
    }

} // namespace vh::websocket
