#include "database/Queries/PermsQueries.hpp"
#include "database/Transactions.hpp"
#include "database/utils.hpp"
#include "types/db/Role.hpp"
#include "../../../../shared/include/types/AssignedRole.hpp"
#include "../../../../shared/include/types/Permission.hpp"
#include "util/timestamp.hpp"

using namespace vh::database;
using namespace vh::types;

// Helper macro for quoting nullable values
#define QUOTE_OR_NULL(val) ((val).has_value() ? txn.quote(*(val)) : "NULL")

void PermsQueries::addRole(const std::shared_ptr<Role>& role) {
    Transactions::exec("PermsQueries::addRole", [&](pqxx::work& txn) {
        txn.exec(
            "INSERT INTO role (name, description, "
            "admin_permissions, vault_permissions, file_permissions, directory_permissions) VALUES (" +
            txn.quote(toSnakeCase(role->name)) + ", " +
            txn.quote(role->description) + ", " +
            bitStringFromMask(role->admin_permissions) + ", " +
            bitStringFromMask(role->vault_permissions) + ", " +
            bitStringFromMask(role->file_permissions) + ", " +
            bitStringFromMask(role->directory_permissions) + ")");
    });
}

void PermsQueries::deleteRole(const unsigned int id) {
    Transactions::exec("PermsQueries::deleteRole", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM role WHERE id = " + txn.quote(id));
    });
}

void PermsQueries::updateRole(const std::shared_ptr<Role>& role) {
    Transactions::exec("PermsQueries::updateRole", [&](pqxx::work& txn) {
        txn.exec(
            "UPDATE role SET "
            "name = " + txn.quote(role->name) + ", " +
            "description = " + txn.quote(role->description) + ", " +
            "admin_permissions = " + bitStringFromMask(role->admin_permissions) + ", " +
            "vault_permissions = " + bitStringFromMask(role->vault_permissions) + ", " +
            "file_permissions = " + bitStringFromMask(role->file_permissions) + ", " +
            "directory_permissions = " + bitStringFromMask(role->directory_permissions) +
            " WHERE id = " + txn.quote(role->id));
    });
}

static std::shared_ptr<Role> makeRoleFromRow(const pqxx::row& row) {
    return std::make_shared<Role>(row);
}

std::shared_ptr<Role> PermsQueries::getRole(unsigned int id) {
    return Transactions::exec("PermsQueries::getRole", [&](pqxx::work& txn) {
        const auto row = txn.exec("SELECT * FROM role WHERE id = " + txn.quote(id)).one_row();
        return makeRoleFromRow(row);
    });
}

std::shared_ptr<Role> PermsQueries::getRoleByName(const std::string& name) {
    return Transactions::exec("PermsQueries::getRoleByName", [&](pqxx::work& txn) {
        const auto row = txn.exec("SELECT * FROM role WHERE name = " + txn.quote(name)).one_row();
        return makeRoleFromRow(row);
    });
}

static const auto* SELECT_ALL_ROLE =
    "SELECT id, name, display_name, description, created_at, "
    "       admin_permissions::int      AS admin_permissions, "
    "       vault_permissions::int      AS vault_permissions, "
    "       file_permissions::int       AS file_permissions, "
    "       directory_permissions::int  AS directory_permissions "
    "FROM role";


std::vector<std::shared_ptr<Role>> PermsQueries::listRoles() {
    return Transactions::exec("PermsQueries::listRoles", [&](pqxx::work& txn) {
        const auto res = txn.exec(SELECT_ALL_ROLE);
        std::vector<std::shared_ptr<Role>> out;
        for (const auto& row : res) out.push_back(makeRoleFromRow(row));
        return out;
    });
}

void PermsQueries::assignUserRole(const std::shared_ptr<AssignedRole>& r) {
    Transactions::exec("PermsQueries::assignUserRole", [&](pqxx::work& txn) {
        txn.exec(
            std::string("INSERT INTO roles (subject_type, subject_id, role_id, scope, scope_id, assigned_at, inherited) VALUES (") +
            "'user', " +
            txn.quote(r->subject_id) + ", " +
            txn.quote(r->role_id) + ", " +
            txn.quote(r->scope) + ", " +
            QUOTE_OR_NULL(r->scope_id) + ", " +
            txn.quote(util::timestampToString(r->assigned_at)) + ", " +
            txn.quote(r->inherited) + ")");
    });
}

void PermsQueries::removeUserRole(const unsigned int userId, unsigned int roleId) {
    Transactions::exec("PermsQueries::removeUserRole", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM roles WHERE subject_id = " + txn.quote(userId) +
                 " AND role_id = " + txn.quote(roleId));
    });
}

static const auto* SELECT_FULL_ROLE =
    "SELECT ur.id                         AS id, "
    "       ur.role_id                    AS role_id, "
    "       ur.subject_id, ur.scope, ur.scope_id, ur.assigned_at, ur.inherited, "
    "       r.name, r.display_name, r.description, r.created_at, "
    "       r.admin_permissions::int      AS admin_permissions, "
    "       r.vault_permissions::int      AS vault_permissions, "
    "       r.file_permissions::int       AS file_permissions, "
    "       r.directory_permissions::int  AS directory_permissions "
    "FROM roles ur JOIN role r ON ur.role_id = r.id ";

std::shared_ptr<Role> PermsQueries::getUserRole(const unsigned int userId, unsigned int roleId) {
    return Transactions::exec("PermsQueries::getUserRole", [&](pqxx::work& txn) {
        const auto row = txn.exec(std::string(SELECT_FULL_ROLE) +
                             "WHERE ur.subject_id = " + txn.quote(userId) +
                             " AND ur.role_id = " + txn.quote(roleId)).one_row();
        return makeRoleFromRow(row);
    });
}

std::vector<std::shared_ptr<Role>> PermsQueries::listUserRoles(const unsigned int userId) {
    return Transactions::exec("PermsQueries::listUserRoles", [&](pqxx::work& txn) {
        const auto res = txn.exec(std::string(SELECT_FULL_ROLE) +
                             "WHERE ur.subject_id = " + txn.quote(userId));
        std::vector<std::shared_ptr<Role>> out;
        for (const auto& row : res) out.push_back(makeRoleFromRow(row));
        return out;
    });
}

std::vector<std::shared_ptr<Role>> PermsQueries::listUserRolesByScope(const std::string& scope, const unsigned int scopeId) {
    return Transactions::exec("PermsQueries::listUserRolesByScope", [&](pqxx::work& txn) {
        const auto res = txn.exec(std::string(SELECT_FULL_ROLE) +
                             "WHERE ur.scope = " + txn.quote(scope) +
                             " AND ur.scope_id = " + txn.quote(scopeId));
        std::vector<std::shared_ptr<Role>> out;
        for (const auto& row : res) out.push_back(makeRoleFromRow(row));
        return out;
    });
}

std::shared_ptr<Permission> PermsQueries::getPermission(unsigned int id) {
    return Transactions::exec("PermsQueries::getPermission", [&](pqxx::work& txn) {
        return std::make_shared<Permission>(txn.exec("SELECT * FROM permissions WHERE id = " + txn.quote(id)).one_row());
    });
}

std::shared_ptr<Permission> PermsQueries::getPermissionByName(const std::string& name) {
    return Transactions::exec("PermsQueries::getPermissionByName", [&](pqxx::work& txn) {
        return std::make_shared<Permission>(txn.exec("SELECT * FROM permissions WHERE name = " + txn.quote(name)).one_row());
    });
}

std::vector<std::shared_ptr<Permission>> PermsQueries::listPermissions() {
    return Transactions::exec("PermsQueries::listPermissions", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM permissions");
        std::vector<std::shared_ptr<Permission>> out;
        for (const auto& row : res) out.push_back(std::make_shared<Permission>(row));
        return out;
    });
}

unsigned short PermsQueries::countPermissions() {
    return Transactions::exec("PermsQueries::countPermissions", [&](pqxx::work& txn) {
        return txn.exec("SELECT COUNT(*) FROM permissions")[0][0].as<unsigned short>();
    });
}

#undef QUOTE_OR_NULL
