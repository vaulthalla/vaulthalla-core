#include "database/Queries/PermsQueries.hpp"
#include "database/Transactions.hpp"
#include "types/db/Role.hpp"
#include "types/db/UserRole.hpp"
#include "types/db/Permission.hpp"
#include "util/bitmask.hpp"
#include "util/timestamp.hpp"

using namespace vh::database;

void PermsQueries::addRole(const std::shared_ptr<types::Role>& role) {
    Transactions::exec("PermsQueries::addRole",
        [&](pqxx::work& txn) {
            txn.exec(
                "INSERT INTO roles (name, description, permissions) "
                "VALUES (" + txn.quote(role->name) + ", " +
                txn.quote(role->description) + ", " +
                txn.quote(util::bitmask::permissions_to_bitstring(role->permissions)) + ", " + ")");
        });
}

void PermsQueries::deleteRole(const unsigned int id) {
    Transactions::exec("PermsQueries::deleteRole",
        [&](pqxx::work& txn) {
            txn.exec("DELETE FROM roles WHERE id = " + txn.quote(id));
        });
}

void PermsQueries::updateRole(const std::shared_ptr<types::Role>& role) {
    Transactions::exec("PermsQueries::updateRole",
        [&](pqxx::work& txn) {
            txn.exec(
                "UPDATE roles SET name = " + txn.quote(role->name) + ", " +
                "description = " + txn.quote(role->description) + ", " +
                "permissions = " + txn.quote(util::bitmask::permissions_to_bitstring(role->permissions)) + ", " +
                " WHERE id = " + txn.quote(role->id));
        });
}

std::shared_ptr<vh::types::Role> PermsQueries::getRole(const unsigned int id) {
    return Transactions::exec("PermsQueries::getRole",
        [&](pqxx::work& txn) -> std::shared_ptr<types::Role> {
            const auto row = txn.exec("SELECT r.id, r.name, r.display_name, r.description, "
                                      "r.permissions::int AS permissions, r.created_at, ur.scope, ur.scope_id, "
                                      "ur.assigned_at FROM roles r "
                                        "JOIN user_roles ur ON r.id = ur.role_id "
                                        "WHERE r.id = " + txn.quote(id)).one_row();

            return std::make_shared<types::Role>(row);
        });
}

std::shared_ptr<vh::types::Role> PermsQueries::getRoleByName(const std::string& name) {
    return Transactions::exec("PermsQueries::getRoleByName",
        [&](pqxx::work& txn) -> std::shared_ptr<types::Role> {
            const auto row = txn.exec("SELECT r.id, r.name, r.display_name, r.description, "
                                      "r.permissions::int AS permissions, r.created_at, ur.scope, ur.scope_id, "
                                      "ur.assigned_at FROM roles r "
                                        "JOIN user_roles ur ON r.id = ur.role_id "
                                        "WHERE r.name = " + txn.quote(name)).one_row();

            return std::make_shared<types::Role>(row);
        });
}

std::vector<std::shared_ptr<vh::types::Role>> PermsQueries::listRoles() {
    return Transactions::exec("PermsQueries::listRoles",
        [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::Role>> {
            const pqxx::result res = txn.exec("SELECT id, name, display_name, description, "
                                              "permissions::int AS permissions, created_at FROM roles");
            std::vector<std::shared_ptr<types::Role>> roles;
            for (const auto& row : res) roles.push_back(std::make_shared<types::Role>(row));
            return roles;
        });
}

void PermsQueries::assignUserRole(const std::shared_ptr<types::UserRole>& userRole) {
    Transactions::exec("PermsQueries::assignUserRole",
        [&](pqxx::work& txn) {
            txn.exec(
                "INSERT INTO user_roles (user_id, role_id, scope, scope_id, assigned_at) "
                "VALUES (" + txn.quote(userRole->user_id) + ", " +
                txn.quote(userRole->role_id) + ", " +
                txn.quote(userRole->scope) + ", " +
                (userRole->scope_id.has_value() ? txn.quote(userRole->scope_id.value()) : "NULL") + ", " +
                txn.quote(util::timestampToString(userRole->assigned_at)) + ")");
        });
}

void PermsQueries::removeUserRole(const unsigned int userId, const unsigned int roleId) {
    Transactions::exec("PermsQueries::removeUserRole",
        [&](pqxx::work& txn) {
            txn.exec("DELETE FROM user_roles WHERE user_id = " + txn.quote(userId) +
                     " AND role_id = " + txn.quote(roleId));
        });
}

std::shared_ptr<vh::types::UserRole> PermsQueries::getUserRole(const unsigned int userId, const unsigned int roleId) {
    return Transactions::exec("PermsQueries::getUserRole",
        [&](pqxx::work& txn) -> std::shared_ptr<types::UserRole> {
            const auto row = txn.exec("SELECT * FROM user_roles WHERE user_id = " + txn.quote(userId) +
                                      " AND role_id = " + txn.quote(roleId)).one_row();
            return std::make_shared<types::UserRole>(row);
        });
}

std::vector<std::shared_ptr<vh::types::UserRole>> PermsQueries::listUserRoles(const unsigned int userId) {
    return Transactions::exec("PermsQueries::listUserRoles",
        [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::UserRole>> {
            const pqxx::result res = txn.exec("SELECT * FROM user_roles WHERE user_id = " + txn.quote(userId));
            std::vector<std::shared_ptr<types::UserRole>> userRoles;
            for (const auto& row : res) userRoles.push_back(std::make_shared<types::UserRole>(row));
            return userRoles;
        });
}

std::vector<std::shared_ptr<vh::types::UserRole>> PermsQueries::listUserRolesByScope(const std::string& scope, unsigned int scopeId) {
    return Transactions::exec("PermsQueries::listUserRolesByScope",
        [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::UserRole>> {
            const pqxx::result res = txn.exec("SELECT * FROM user_roles WHERE scope = " + txn.quote(scope) +
                                              " AND scope_id = " + txn.quote(scopeId));
            std::vector<std::shared_ptr<types::UserRole>> userRoles;
            for (const auto& row : res) userRoles.push_back(std::make_shared<types::UserRole>(row));
            return userRoles;
        });
}

std::shared_ptr<vh::types::Permission> PermsQueries::getPermission(const unsigned int id) {
    return Transactions::exec("PermsQueries::getPermission",
        [&](pqxx::work& txn) -> std::shared_ptr<types::Permission> {
            const auto row = txn.exec("SELECT * FROM permissions WHERE id = " + txn.quote(id)).one_row();
            return std::make_shared<types::Permission>(row);
        });
}

std::shared_ptr<vh::types::Permission> PermsQueries::getPermissionByName(const std::string& name) {
    return Transactions::exec("PermsQueries::getPermissionByName",
        [&](pqxx::work& txn) -> std::shared_ptr<types::Permission> {
            const auto row = txn.exec("SELECT * FROM permissions WHERE name = " + txn.quote(name)).one_row();
            return std::make_shared<types::Permission>(row);
        });
}

std::vector<std::shared_ptr<vh::types::Permission>> PermsQueries::listPermissions() {
    return Transactions::exec("PermsQueries::listPermissions",
        [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::Permission>> {
            const pqxx::result res = txn.exec("SELECT * FROM permissions");
            std::vector<std::shared_ptr<types::Permission>> permissions;
            for (const auto& row : res) permissions.push_back(std::make_shared<types::Permission>(row));
            return permissions;
        });
}

unsigned short PermsQueries::countPermissions() {
    return Transactions::exec("PermsQueries::countPermissions",
        [&](pqxx::work& txn) -> unsigned short {
            const pqxx::result res = txn.exec("SELECT COUNT(*) FROM permissions");
            return res[0][0].as<unsigned short>();
        });
}
