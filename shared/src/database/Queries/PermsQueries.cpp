#include "../../../../shared/include/database/Queries/PermsQueries.hpp"
#include "../../../../shared/include/database/Transactions.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"
#include "types/Permission.hpp"

using namespace vh::database;
using namespace vh::types;

void PermsQueries::addRole(const std::shared_ptr<Role>& role) {
    Transactions::exec("PermsQueries::addRole", [&](pqxx::work& txn) {
        txn.exec_prepared("insert_role", pqxx::params{role->name, role->description, role->type});
    });
}

void PermsQueries::deleteRole(const unsigned int id) {
    Transactions::exec("PermsQueries::deleteRole", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_role", pqxx::params{id});
    });
}

void PermsQueries::updateRole(const std::shared_ptr<Role>& role) {
    Transactions::exec("PermsQueries::updateRole", [&](pqxx::work& txn) {
        pqxx::params p{role->role_id, role->name, role->description, role->type};
        txn.exec_prepared("update_role", p);

        pqxx::params perms_params{role->role_id, role->permissions};
        txn.exec_prepared("upsert_permissions", perms_params);
    });
}

std::shared_ptr<Role> PermsQueries::getRole(const unsigned int id) {
    return Transactions::exec("PermsQueries::getRole", [&](pqxx::work& txn) {
        const auto row = txn.exec("SELECT * FROM role WHERE id = " + txn.quote(id)).one_row();
        return std::make_shared<Role>(row);
    });
}

std::shared_ptr<Role> PermsQueries::getRoleByName(const std::string& name) {
    return Transactions::exec("PermsQueries::getRoleByName", [&](pqxx::work& txn) {
        const auto row = txn.exec("SELECT * FROM role WHERE name = " + txn.quote(name)).one_row();
        return std::make_shared<Role>(row);
    });
}


std::vector<std::shared_ptr<Role>> PermsQueries::listRoles() {
    return Transactions::exec("PermsQueries::listRoles", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_roles");
        return roles_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<Role>> PermsQueries::listUserRoles() {
    return Transactions::exec("PermsQueries::listUserRoles", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_roles_by_type", pqxx::params{"user"});
        return roles_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<Role>> PermsQueries::listVaultRoles() {
    return Transactions::exec("PermsQueries::listFSRoles", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_roles_by_type", pqxx::params{"vault"});
        return roles_from_pq_res(res);
    });
}

void PermsQueries::assignVaultRole(const std::shared_ptr<VaultRole>& roleAssignment) {
    Transactions::exec("PermsQueries::assignRole", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(roleAssignment->subject_type);
        p.append(roleAssignment->subject_id);
        p.append(roleAssignment->vault_id);
        p.append(roleAssignment->role_id);

        txn.exec_prepared("assign_vault_role", p);
    });
}

void PermsQueries::removeVaultRoleAssignment(const unsigned int id) {
    Transactions::exec("PermsQueries::removeAssignedRole", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_vault_role_assignment", pqxx::params{id});
    });
}

std::shared_ptr<VaultRole> PermsQueries::getVaultRoleBySubject(const unsigned int subjectId, const std::string& subjectType, const unsigned int roleId) {
    return Transactions::exec("PermsQueries::getSubjectAssignedRole", [&](pqxx::work& txn) {
        pqxx::params role_params{subjectType, subjectId, roleId};
        const auto role = txn.exec_prepared("get_subject_assigned_vault_role", role_params).one_row();
        const auto overrides = txn.exec_prepared("get_vault_permission_overrides", pqxx::params{roleId});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::shared_ptr<VaultRole> PermsQueries::getVaultRole(const unsigned int id) {
    return Transactions::exec("PermsQueries::getAssignedRole", [&](pqxx::work& txn) {
        const auto role = txn.exec_prepared("get_vault_assigned_role", pqxx::params{id}).one_row();
        const auto overrides = txn.exec_prepared("get_assigned_role_overrides", pqxx::params{id});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::vector<std::shared_ptr<VaultRole>> PermsQueries::listVaultAssignedRoles(const unsigned int vaultId) {
    return Transactions::exec("PermsQueries::listAssignedRoles", [&](pqxx::work& txn) {
        pqxx::params p{vaultId};
        const auto roles = txn.exec_prepared("get_vault_assigned_roles", p);
        const auto overrides = txn.exec_prepared("get_vault_permissions_overrides", p);
        return vault_roles_from_pq_result(roles, overrides);
    });
}

std::shared_ptr<Permission> PermsQueries::getPermission(const unsigned int id) {
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
