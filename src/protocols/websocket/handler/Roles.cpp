#include "protocols/ws/handler/Roles.hpp"
#include "protocols/ws/Session.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "identities/model/User.hpp"
#include "rbac/model/Role.hpp"
#include "rbac/model/VaultRole.hpp"

using namespace vh::protocols::ws::handler;
using namespace vh::rbac::model;
using namespace vh::database;

json Roles::add(const json& payload, const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can add roles");

    const auto role = std::make_shared<Role>(payload);
    role->id = PermsQueries::addRole(role);
    return {{"role", *role}};
}

json Roles::remove(const json& payload, const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can delete roles");

    const auto roleId = payload.at("id").get<unsigned int>();
    PermsQueries::deleteRole(roleId);
    return {{"role", roleId}};
}

json Roles::update(const json& payload, const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can update roles");

    auto role = std::make_shared<Role>(payload);
    PermsQueries::updateRole(role);
    return {{"role", *role}};
}

json Roles::get(const json& payload, const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get roles");

    const auto roleId = payload.at("id").get<unsigned int>();
    auto role = PermsQueries::getRole(roleId);

    if (!role) throw std::runtime_error("Role not found");

    return {{"role", *role}};
}

json Roles::getByName(const json& payload, const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can get roles by name");

    const auto roleName = payload.at("name").get<std::string>();
    auto role = PermsQueries::getRoleByName(roleName);
    if (!role) throw std::runtime_error("Role not found");
    return {{"role", *role}};
}

json Roles::list(const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list roles");

    return {{"roles", PermsQueries::listRoles()}};
}

json Roles::listUserRoles(const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list user roles");

    return {{"roles", PermsQueries::listUserRoles()}};
}

json Roles::listVaultRoles(const Session& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageRoles())
        throw std::runtime_error("Permission denied: Only admins can list filesystem roles");

    return {{"roles", PermsQueries::listVaultRoles()}};
}
