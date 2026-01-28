#include "protocols/websocket/handlers/RolesHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "types/User.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"

using namespace vh::websocket;

json RolesHandler::add(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can add roles");

    const auto role = std::make_shared<types::Role>(payload);
    role->id = database::PermsQueries::addRole(role);
    return {{"role", *role}};
}

json RolesHandler::remove(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can delete roles");

    const auto roleId = payload.at("id").get<unsigned int>();
    database::PermsQueries::deleteRole(roleId);
    return {{"role", roleId}};
}

json RolesHandler::update(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can update roles");

    auto role = std::make_shared<types::Role>(payload);
    database::PermsQueries::updateRole(role);
    return {{"role", *role}};
}

json RolesHandler::get(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get roles");

    const auto roleId = payload.at("id").get<unsigned int>();
    auto role = database::PermsQueries::getRole(roleId);

    if (!role) throw std::runtime_error("Role not found");

    return {{"role", *role}};
}

json RolesHandler::getByName(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get roles by name");

    const auto roleName = payload.at("name").get<std::string>();
    auto role = database::PermsQueries::getRoleByName(roleName);
    if (!role) throw std::runtime_error("Role not found");
    return {{"role", *role}};
}

json RolesHandler::list(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list roles");

    return {{"roles", database::PermsQueries::listRoles()}};
}

json RolesHandler::listUserRoles(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list user roles");

    return {{"roles", database::PermsQueries::listUserRoles()}};
}

json RolesHandler::listVaultRoles(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list filesystem roles");

    return {{"roles", database::PermsQueries::listVaultRoles()}};
}
