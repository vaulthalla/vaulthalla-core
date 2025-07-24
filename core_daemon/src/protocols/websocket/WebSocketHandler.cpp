#include "protocols/websocket/WebSocketHandler.hpp"
#include "protocols/websocket/handlers/PermissionsHandler.hpp"
#include "protocols/websocket/handlers/SettingsHandler.hpp"
#include "protocols/websocket/handlers/GroupHandler.hpp"
#include <iostream>

namespace vh::websocket {

WebSocketHandler::WebSocketHandler(const std::shared_ptr<services::ServiceManager>& serviceManager,
                                   const std::shared_ptr<WebSocketRouter>& router)
    : router_(router), serviceManager_(serviceManager) {
    authHandler_ = std::make_shared<AuthHandler>(serviceManager_->authManager());
    storageHandler_ = std::make_shared<StorageHandler>(serviceManager_->storageManager());
    fsHandler_ = std::make_shared<FileSystemHandler>(serviceManager_);
    shareHandler_ = std::make_shared<ShareHandler>(serviceManager_->linkResolver());
    searchHandler_ = std::make_shared<SearchHandler>(serviceManager_->searchIndex());
    notificationHandler_ = std::make_shared<NotificationHandler>();
    registerAllHandlers();
}

void WebSocketHandler::registerAllHandlers() const {
    registerAuthHandlers();
    registerFileSystemHandlers();
    registerStorageHandlers();
    registerAPIKeyHandlers();
    registerPermissionsHandlers();
    registerSettingsHandlers();
    registerGroupHandlers();

    std::cout << "[WebSocketHandler] All handlers registered.\n";
}

void WebSocketHandler::registerAuthHandlers() const {
    router_->registerHandler("auth.login", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleLogin(msg, session);
    });

    router_->registerHandler("auth.refresh", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleRefresh(msg, session);
    });

    router_->registerHandler("auth.logout", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleLogout(msg, session);
    });

    router_->registerHandler("auth.user.update", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleUpdateUser(msg, session);
    });

    router_->registerHandler("auth.user.change_password", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleChangePassword(msg, session);
    });

    router_->registerHandler("auth.isAuthenticated", [this](const json& msg, WebSocketSession& session) {
        authHandler_->isUserAuthenticated(msg, session);
    });

    router_->registerHandler("auth.register", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleRegister(msg, session);
    });

    router_->registerHandler("auth.user.get", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleGetUser(msg, session);
    });

    router_->registerHandler("auth.users.list", [this](const json& msg, WebSocketSession& session) {
        authHandler_->handleListUsers(msg, session);
    });
}

void WebSocketHandler::registerFileSystemHandlers() const {
    router_->registerHandler("fs.upload.start", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleUploadStart(msg, session);
    });

    router_->registerHandler("fs.upload.finish", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleUploadFinish(msg, session);
    });

    router_->registerHandler("fs.dir.create", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleMkdir(msg, session);
    });

    router_->registerHandler("fs.dir.list", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleListDir(msg, session);
    });

    router_->registerHandler("fs.file.read", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleReadFile(msg, session);
    });

    router_->registerHandler("fs.entry.delete", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleDelete(msg, session);
    });

    router_->registerHandler("fs.entry.move", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleMove(msg, session);
    });

    router_->registerHandler("fs.entry.rename", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleRename(msg, session);
    });

    router_->registerHandler("fs.entry.copy", [this](const json& msg, WebSocketSession& session) {
        fsHandler_->handleCopy(msg, session);
    });
}

void WebSocketHandler::registerStorageHandlers() const {
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

    router_->registerHandler("storage.vault.sync", [this](const json& msg, WebSocketSession& session) {
        storageHandler_->handleSyncVault(msg, session);
    });
}

void WebSocketHandler::registerAPIKeyHandlers() const {
    router_->registerHandler("storage.apiKey.add", [this](const json& msg, WebSocketSession& session) {
       storageHandler_->handleAddAPIKey(msg, session);
   });

    router_->registerHandler("storage.apiKey.remove", [this](const json& msg, WebSocketSession& session) {
        storageHandler_->handleRemoveAPIKey(msg, session);
    });

    router_->registerHandler("storage.apiKey.list", [this](const json& msg, WebSocketSession& session) {
        storageHandler_->handleListAPIKeys(msg, session);
    });

    router_->registerHandler("storage.apiKey.list.user", [this](const json& msg, WebSocketSession& session) {
        storageHandler_->handleListUserAPIKeys(msg, session);
    });

    router_->registerHandler("storage.apiKey.get", [this](const json& msg, WebSocketSession& session) {
        storageHandler_->handleGetAPIKey(msg, session);
    });
}

void WebSocketHandler::registerPermissionsHandlers() const {
    router_->registerHandler("role.add", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleAddRole(msg, session);
    });

    router_->registerHandler("role.delete", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleDeleteRole(msg, session);
    });

    router_->registerHandler("role.update", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleUpdateRole(msg, session);
    });

    router_->registerHandler("role.get", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleGetRole(msg, session);
    });

    router_->registerHandler("role.get.byName", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleGetRoleByName(msg, session);
    });

    router_->registerHandler("roles.list", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleListRoles(msg, session);
    });

    router_->registerHandler("roles.list.user", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleListUserRoles(msg, session);
    });

    router_->registerHandler("roles.list.vault", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleListVaultRoles(msg, session);
    });

    router_->registerHandler("permission.get", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleGetPermission(msg, session);
    });

    router_->registerHandler("permission.get.byName", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleGetPermissionByName(msg, session);
    });

    router_->registerHandler("permissions.list", [this](const json& msg, WebSocketSession& session) {
        PermissionsHandler::handleListPermissions(msg, session);
    });
}

void WebSocketHandler::registerSettingsHandlers() const {
    router_->registerHandler("settings.get", [this](const json& msg, WebSocketSession& session) {
        SettingsHandler::handleGetSettings(msg, session);
    });

    router_->registerHandler("settings.update", [this](const json& msg, WebSocketSession& session) {
        SettingsHandler::handleUpdateSettings(msg, session);
    });
}

void WebSocketHandler::registerGroupHandlers() const {
    router_->registerHandler("group.add", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleCreateGroup(msg, session);
    });
    router_->registerHandler("group.remove", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleDeleteGroup(msg, session);
    });
    router_->registerHandler("group.member.add", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleAddMemberToGroup(msg, session);
    });
    router_->registerHandler("group.member.remove", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleRemoveMemberFromGroup(msg, session);
    });
    router_->registerHandler("group.update", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleUpdateGroup(msg, session);
    });
    router_->registerHandler("groups.list", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleListGroups(msg, session);
    });
    router_->registerHandler("group.get", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleGetGroup(msg, session);
    });
    router_->registerHandler("group.get.byName", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleGetGroupByName(msg, session);
    });
    router_->registerHandler("groups.list.byUser", [this](const json& msg, WebSocketSession& session) {
        GroupHandler::handleListGroupsByUser(msg, session);
    });
}


} // namespace vh::websocket
