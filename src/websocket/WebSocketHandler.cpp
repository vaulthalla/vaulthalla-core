#include "websocket/WebSocketHandler.hpp"
#include "auth/SessionManager.hpp"

#include <iostream>

namespace vh::websocket {

    WebSocketHandler::WebSocketHandler(const std::shared_ptr<vh::services::ServiceManager> &serviceManager,
                                       const std::shared_ptr<WebSocketRouter> &router)
                   : router_(router), serviceManager_(serviceManager) {
        authHandler_ = std::make_shared<AuthHandler>(serviceManager_->authManager());
        storageHandler_ = std::make_shared<StorageHandler>(serviceManager_->storageManager());
        fsHandler_ = std::make_shared<FileSystemHandler>(serviceManager_);
        shareHandler_ = std::make_shared<ShareHandler>(serviceManager_->linkResolver());
        searchHandler_ = std::make_shared<SearchHandler>(serviceManager_->searchIndex());
        notificationHandler_ = std::make_shared<NotificationHandler>();
        registerAllHandlers();
    }

    void WebSocketHandler::registerAllHandlers() {
        // Auth
        router_->registerHandler("auth.login", [this](const json& msg, WebSocketSession& session) {
            authHandler_->handleLogin(msg, session);
        });

        router_->registerHandler("auth.refresh", [this](const json& msg, WebSocketSession& session) {
            authHandler_->handleRefresh(msg, session);
        });

        router_->registerHandler("auth.logout", [this](const json& msg, WebSocketSession& session) {
            authHandler_->handleLogout(msg, session);
        });

        router_->registerHandler("auth.isAuthenticated", [this](const json& msg, WebSocketSession& session) {
            authHandler_->isUserAuthenticated(msg, session);
        });

        router_->registerHandler("auth.register", [this](const json& msg, WebSocketSession& session) {
            authHandler_->handleRegister(msg, session);
        });

        // FileSystem
        router_->registerHandler("fs.listDir", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleListDir(msg, session);
        });

        router_->registerHandler("fs.readFile", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleReadFile(msg, session);
        });

        router_->registerHandler("fs.writeFile", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleWriteFile(msg, session);
        });

        router_->registerHandler("fs.deleteFile", [this](const json& msg, WebSocketSession& session) {
            fsHandler_->handleDeleteFile(msg, session);
        });

        // Storage
        router_->registerHandler("storage.vault.list", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleListVaults(msg, session);
        });

        router_->registerHandler("storage.vault.add", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleAddVault(msg, session);
        });

        router_->registerHandler("storage.vault.remove", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleRemoveVault(msg, session);
        });

        router_->registerHandler("storage.vault.get", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleGetVault(msg, session);
        });

        router_->registerHandler("storage.volume.add", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleAddVolume(msg, session);
        });

        router_->registerHandler("storage.volume.remove", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleRemoveVolume(msg, session);
        });

        router_->registerHandler("storage.volume.list", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleListVolumes(msg, session);
        });

        router_->registerHandler("storage.volume.get", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleGetVolume(msg, session);
        });

        // API Keys

        router_->registerHandler("storage.apiKey.add", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleAddAPIKey(msg, session);
        });

        router_->registerHandler("storage.apiKye.remove", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleRemoveAPIKey(msg, session);
        });

        router_->registerHandler("storage.apiKey.list", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleListAPIKeys(msg, session);
        });

        router_->registerHandler("storage.apiKey.get", [this](const json& msg, WebSocketSession& session) {
            storageHandler_->handleGetAPIKey(msg, session);
        });

        // Share
        router_->registerHandler("share.createLink", [this](const json& msg, WebSocketSession& session) {
            shareHandler_->handleCreateLink(msg, session);
        });

        router_->registerHandler("share.resolveLink", [this](const json& msg, WebSocketSession& session) {
            shareHandler_->handleResolveLink(msg, session);
        });

        // Search
        router_->registerHandler("index.search", [this](const json& msg, WebSocketSession& session) {
            searchHandler_->handleSearch(msg, session);
        });

        // Notifications
        router_->registerHandler("notification.subscribe", [this](const json& msg, WebSocketSession& session) {
            notificationHandler_->handleSubscribe(msg, session);
        });

        router_->registerHandler("notification.unsubscribe", [this](const json& msg, WebSocketSession& session) {
            notificationHandler_->handleUnsubscribe(msg, session);
        });

        std::cout << "[WebSocketHandler] All handlers registered.\n";
    }

} // namespace vh::websocket
