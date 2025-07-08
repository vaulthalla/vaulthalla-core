#include "protocols/websocket/handlers/PermissionsHandler.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "types/User.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"
#include "types/Permission.hpp"

#include <nlohmann/json.hpp>

using namespace vh::websocket;

void PermissionsHandler::handleAddRole(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can add roles");

        const auto& payload = msg.at("payload");
        auto role = std::make_shared<types::Role>(payload);

        database::PermsQueries::addRole(role);

        const json data = {{"role", *role}};

        const json response = {{"command", "role.add.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "role.add.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleDeleteRole(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can delete roles");

        const auto roleId = msg.at("payload").at("id").get<unsigned int>();
        database::PermsQueries::deleteRole(roleId);

        const json response = {{"command", "role.delete.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "role.delete.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleUpdateRole(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can update roles");

        const auto& payload = msg.at("payload");
        auto role = std::make_shared<types::Role>(payload);

        database::PermsQueries::updateRole(role);

        const json data = {{"role", *role}};

        const json response = {{"command", "role.update.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "role.update.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleGetRole(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can get roles");

        const auto roleId = msg.at("payload").at("id").get<unsigned int>();
        auto role = database::PermsQueries::getRole(roleId);

        if (!role) throw std::runtime_error("Role not found");

        const json data = {{"role", *role}};

        const json response = {{"command", "role.get.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "role.get.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleGetRoleByName(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can get roles by name");

        const auto roleName = msg.at("payload").at("name").get<std::string>();
        auto role = database::PermsQueries::getRoleByName(roleName);

        if (!role) throw std::runtime_error("Role not found");

        const json data = {{"role", *role}};

        const json response = {{"command", "role.get.byName.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "role.get.byName.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleListRoles(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can list roles");

        const auto roles = database::PermsQueries::listRoles();

        const json data = {{"roles", roles}};

        const json response = {{"command", "roles.list.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "roles.list.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleListUserRoles(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can list user roles");

        const auto roles = database::PermsQueries::listUserRoles();

        const json data = {{"roles", roles}};

        const json response = {{"command", "roles.list.user.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "roles.list.user.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleListVaultRoles(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can list filesystem roles");

        const auto roles = database::PermsQueries::listVaultRoles();

        const json data = {{"roles", roles}};

        const json response = {{"command", "roles.list.vault.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "roles.list.vault.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleGetPermission(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can get permissions");

        const auto permissionId = msg.at("payload").at("id").get<unsigned int>();
        auto permission = database::PermsQueries::getPermission(permissionId);

        if (!permission) throw std::runtime_error("Permission not found");

        const json data = {{"permission", *permission}};

        const json response = {{"command", "permission.get.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "permission.get.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleGetPermissionByName(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can get permissions by name");

        const auto permissionName = msg.at("payload").at("name").get<std::string>();
        auto permission = database::PermsQueries::getPermissionByName(permissionName);

        if (!permission) throw std::runtime_error("Permission not found");

        const json data = {{"permission", *permission}};

        const json response = {{"command", "permission.get.byName.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "permission.get.byName.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}

void PermissionsHandler::handleListPermissions(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can list permissions");

        auto permissions = database::PermsQueries::listPermissions();

        const json data = {{"permissions", permissions}};

        const json response = {{"command", "permissions.list.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {{"command", "permissions.list.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};
        session.send(response);
    }
}
