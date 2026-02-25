#include "protocols/ws/handler/Permissions.hpp"
#include "protocols/ws/Session.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "identities/model/User.hpp"
#include "rbac/model/Permission.hpp"

#include <nlohmann/json.hpp>

using namespace vh::protocols::ws::handler;
using namespace vh::database;

json Permissions::get(const json& payload, const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get permissions");

    const auto permissionId = payload.at("id").get<unsigned int>();
    auto permission = PermsQueries::getPermission(permissionId);
    if (!permission) throw std::runtime_error("Permission not found");

    return {{"permission", *permission}};
}

json Permissions::getByName(const json& payload, const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get permissions by name");

    const auto permissionName = payload.at("name").get<std::string>();
    auto permission = PermsQueries::getPermissionByName(permissionName);
    if (!permission) throw std::runtime_error("Permission not found");

    return {{"permission", *permission}};
}

json Permissions::list(const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list permissions");

    auto permissions = PermsQueries::listPermissions();

    return {{"permissions", permissions}};
}
