#include "protocols/websocket/WebSocketHandler.hpp"
#include "protocols/websocket/WebSocketRouter.hpp"
#include "protocols/websocket/handlers/PermissionsHandler.hpp"
#include "protocols/websocket/handlers/SettingsHandler.hpp"
#include "protocols/websocket/handlers/GroupHandler.hpp"
#include "protocols/websocket/handlers/StatsHandler.hpp"
#include "protocols/websocket/handlers/RolesHandler.hpp"
#include "protocols/websocket/handlers/StorageHandler.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::services;
using namespace vh::logging;

namespace vh::websocket {

WebSocketHandler::WebSocketHandler(const std::shared_ptr<WebSocketRouter>& router)
    : router_(router) { registerAllHandlers(); }

void WebSocketHandler::registerAllHandlers() const {
    registerAuthHandlers();
    registerFileSystemHandlers();
    registerStorageHandlers();
    registerAPIKeyHandlers();
    registerRoleHandlers();
    registerPermissionsHandlers();
    registerSettingsHandlers();
    registerGroupHandlers();
    registerStatHandlers();

    LogRegistry::ws()->debug("[WebSocketHandler] All handlers registered successfully.");
}

void WebSocketHandler::registerAuthHandlers() const {
    router_->registerPayload("auth.login", &AuthHandler::login);
    router_->registerPayload("auth.register", &AuthHandler::registerUser);
    router_->registerSessionOnlyHandler("auth.logout", &AuthHandler::logout);
    router_->registerPayload("auth.user.update", &AuthHandler::updateUser);
    router_->registerPayload("auth.user.change_password", &AuthHandler::changePassword);
    router_->registerHandlerWithToken("auth.isAuthenticated", &AuthHandler::isUserAuthenticated);
    router_->registerPayload("auth.user.get", &AuthHandler::getUser);
    router_->registerPayload("auth.user.get.byName", &AuthHandler::getUserByName);
    router_->registerSessionOnlyHandler("auth.users.list", &AuthHandler::listUsers);
    router_->registerEmptyHandler("auth.admin.default_password", &AuthHandler::doesAdminHaveDefaultPassword);
    router_->registerSessionOnlyHandler("auth.refresh", &AuthHandler::refresh);
}

void WebSocketHandler::registerFileSystemHandlers() const {
    router_->registerPayload("fs.upload.start", &FileSystemHandler::startUpload);
    router_->registerPayload("fs.upload.finish", &FileSystemHandler::finishUpload);
    router_->registerPayload("fs.dir.create", &FileSystemHandler::mkdir);
    router_->registerPayload("fs.dir.list", &FileSystemHandler::listDir);
    router_->registerPayload("fs.entry.delete", &FileSystemHandler::remove);
    router_->registerPayload("fs.entry.move", &FileSystemHandler::move);
    router_->registerPayload("fs.entry.rename", &FileSystemHandler::rename);
    router_->registerPayload("fs.entry.copy", &FileSystemHandler::copy);
}

void WebSocketHandler::registerStorageHandlers() const {
    router_->registerSessionOnlyHandler("storage.vault.list", &StorageHandler::listVaults);
    router_->registerPayload("storage.vault.add", &StorageHandler::addVault);
    router_->registerPayload("storage.vault.update", &StorageHandler::updateVault);
    router_->registerPayload("storage.vault.remove", &StorageHandler::removeVault);
    router_->registerPayload("storage.vault.get", &StorageHandler::getVault);
    router_->registerPayload("storage.vault.sync", &StorageHandler::syncVault);
}

void WebSocketHandler::registerAPIKeyHandlers() const {
    router_->registerPayload("storage.apiKey.add", &StorageHandler::addAPIKey);
    router_->registerPayload("storage.apiKey.remove", &StorageHandler::removeAPIKey);
    router_->registerSessionOnlyHandler("storage.apiKey.list", &StorageHandler::listAPIKeys);
    router_->registerPayload("storage.apiKey.get", &StorageHandler::getAPIKey);
}

void WebSocketHandler::registerRoleHandlers() const {
    router_->registerPayload("role.add", &RolesHandler::add);
    router_->registerPayload("role.update", &RolesHandler::update);
    router_->registerPayload("role.delete", &RolesHandler::remove);
    router_->registerPayload("role.get", &RolesHandler::get);
    router_->registerPayload("role.get.byName", &RolesHandler::getByName);
    router_->registerSessionOnlyHandler("roles.list", &RolesHandler::list);
    router_->registerSessionOnlyHandler("roles.list.user", &RolesHandler::listUserRoles);
    router_->registerSessionOnlyHandler("roles.list.vault", &RolesHandler::listVaultRoles);
}

void WebSocketHandler::registerPermissionsHandlers() const {
    router_->registerPayload("permission.get", &PermissionsHandler::get);
    router_->registerPayload("permission.get.byName", &PermissionsHandler::getByName);
    router_->registerSessionOnlyHandler("permissions.list", &PermissionsHandler::list);
}

void WebSocketHandler::registerSettingsHandlers() const {
    router_->registerSessionOnlyHandler("settings.get", &SettingsHandler::get);
    router_->registerPayload("settings.update", &SettingsHandler::update);
}

void WebSocketHandler::registerGroupHandlers() const {
    router_->registerPayload("group.add", &GroupHandler::add);
    router_->registerPayload("group.update", &GroupHandler::update);
    router_->registerPayload("group.remove", &GroupHandler::remove);
    router_->registerPayload("group.member.add", &GroupHandler::addMember);
    router_->registerPayload("group.member.remove", &GroupHandler::removeMember);
    router_->registerPayload("group.get", &GroupHandler::get);
    router_->registerPayload("group.get.byName", &GroupHandler::getByName);
    router_->registerPayload("groups.list.byUser", &GroupHandler::listByUser);
    router_->registerSessionOnlyHandler("groups.list", &GroupHandler::list);
}

void WebSocketHandler::registerStatHandlers() const {
    router_->registerPayload("stats.vault", &StatsHandler::vault);
    router_->registerSessionOnlyHandler("stats.fs.cache", &StatsHandler::fsCache);
    router_->registerSessionOnlyHandler("stats.http.cache", &StatsHandler::httpCache);
}

} // namespace vh::websocket