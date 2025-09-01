#include "database/Queries/PermsQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"
#include "types/Permission.hpp"

using namespace vh::database;
using namespace vh::types;

// #################################################################################################
// ############################################ Role  #############################################
// #################################################################################################

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
        pqxx::params p{role->id, role->name, role->description, role->type};
        txn.exec_prepared("update_role", p);

        pqxx::params perms_params{role->id, role->permissions};
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
        const auto res = txn.exec_prepared("get_role_by_name", pqxx::params{name});
        if (res.empty()) throw std::runtime_error("Role not found: " + name);
        return std::make_shared<Role>(res.one_row());
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

// #####################################################################################################
// ############################################# Vault Role  ###########################################
// #####################################################################################################

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

// #####################################################################################################
// ##################################### Vault Permission Override  ####################################
// #####################################################################################################

unsigned int PermsQueries::addVPermOverride(const std::shared_ptr<PermissionOverride>& override) {
    return Transactions::exec("PermsQueries::addVPermOverride", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(override->assignment_id);
        p.append(override->permission.id);
        p.append(override->patternStr);
        p.append(override->enabled);
        p.append(to_string(override->effect));

        const auto res = txn.exec_prepared("insert_vault_permission_override", p);
        if (res.empty()) throw std::runtime_error("Failed to insert vault permission override");
        return res[0][0].as<unsigned int>();
    });
}

void PermsQueries::updateVPermOverride(const std::shared_ptr<PermissionOverride>& override) {
    Transactions::exec("PermsQueries::updateVPermOverride", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(override->id);
        p.append(override->patternStr);
        p.append(override->enabled);
        p.append(to_string(override->effect));

        txn.exec_prepared("update_vault_permission_override", p);
    });
}

void PermsQueries::removeVPermOverride(const unsigned int permOverrideId) {
    Transactions::exec("PermsQueries::removeVPermOverride", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_vault_permission_override", pqxx::params{permOverrideId});
    });
}

std::vector<std::shared_ptr<PermissionOverride>> PermsQueries::listVPermOverrides(const unsigned int vaultId) {
    return Transactions::exec("PermsQueries::listVPermOverrides", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_vault_permission_overrides", pqxx::params{vaultId});
        return permissionOverridesFromPqRes(res);
    });
}

std::vector<std::shared_ptr<PermissionOverride>> PermsQueries::listAssignedVRoleOverrides(const VPermOverrideQuery& query) {
    return Transactions::exec("PermsQueries::listAssignedVRoleOverrides", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(query.vault_id);
        p.append(query.subject_type);
        p.append(query.subject_id);

        const auto res = txn.exec_prepared("list_subject_permission_overrides", p);
        return permissionOverridesFromPqRes(res);
    });
}

std::shared_ptr<PermissionOverride> PermsQueries::getVPermOverride(const VPermOverrideQuery& query) {
    return Transactions::exec("PermsQueries::getVPermOverride", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(query.vault_id);
        p.append(query.subject_type);
        p.append(query.subject_id);
        p.append(query.bit_position);

        const auto res = txn.exec_prepared("get_permission_override_by_vault_subject_and_bitpos", p);
        if (res.empty()) return std::shared_ptr<PermissionOverride>(nullptr);
        return std::make_shared<PermissionOverride>(res.one_row());
    });
}


// #################################################################################################
// ########################################## Permission  ##########################################
// #################################################################################################

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
