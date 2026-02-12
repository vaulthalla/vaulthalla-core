#include "protocols/websocket/handlers/PermissionsHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "types/entities/User.hpp"

#include <nlohmann/json.hpp>

using namespace vh::websocket;
using namespace vh::database;

json PermissionsHandler::get(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get permissions");

    const auto permissionId = payload.at("id").get<unsigned int>();
    auto permission = PermsQueries::getPermission(permissionId);
    if (!permission) throw std::runtime_error("Permission not found");

    return {{"permission", *permission}};
}

json PermissionsHandler::getByName(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get permissions by name");

    const auto permissionName = payload.at("name").get<std::string>();
    auto permission = database::PermsQueries::getPermissionByName(permissionName);
    if (!permission) throw std::runtime_error("Permission not found");

    return {{"permission", *permission}};
}

json PermissionsHandler::list(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list permissions");

    auto permissions = database::PermsQueries::listPermissions();

    return {{"permissions", permissions}};
}
