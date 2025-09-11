#include "database/Queries/PermsQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"
#include "types/Permission.hpp"
#include "types/PermissionOverride.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::database;
using namespace vh::types;
using namespace vh::logging;

// #################################################################################################
// ############################################ Role  #############################################
// #################################################################################################

unsigned int PermsQueries::addRole(const std::shared_ptr<Role>& role) {
    return Transactions::exec("PermsQueries::addRole", [&](pqxx::work& txn) {
        return txn.exec(pqxx::prepped{"insert_role"},
            pqxx::params{role->name, role->description, role->type}).one_field().as<unsigned int>();
    });
}

void PermsQueries::deleteRole(const unsigned int id) {
    Transactions::exec("PermsQueries::deleteRole", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_role"}, pqxx::params{id});
    });
}

void PermsQueries::updateRole(const std::shared_ptr<Role>& role) {
    Transactions::exec("PermsQueries::updateRole", [&](pqxx::work& txn) {
        pqxx::params p{role->id, role->name, role->description, role->type};
        txn.exec(pqxx::prepped{"update_role"}, p);

        LogRegistry::db()->trace("[PermsQueries] Updating permissions for role ID {} to mask {}",
            role->id, bitStringFromMask(role->permissions));

        const pqxx::params perms_params{role->id, bitStringFromMask(role->permissions)};
        txn.exec(pqxx::prepped{"update_role_permissions"}, perms_params);
    });
}

std::shared_ptr<Role> PermsQueries::getRole(const unsigned int id) {
    return Transactions::exec("PermsQueries::getRole", [&](pqxx::work& txn) -> std::shared_ptr<Role> {
        const auto res = txn.exec(pqxx::prepped{"get_role"}, pqxx::params{id});
        if (res.empty()) return nullptr;
        return std::make_shared<Role>(res.one_row());
    });
}

std::shared_ptr<Role> PermsQueries::getRoleByName(const std::string& name) {
    return Transactions::exec("PermsQueries::getRoleByName", [&](pqxx::work& txn) -> std::shared_ptr<Role> {
        const auto res = txn.exec(pqxx::prepped{"get_role_by_name"}, pqxx::params{name});
        if (res.empty()) return nullptr;
        return std::make_shared<Role>(res.one_row());
    });
}

bool PermsQueries::roleExists(const std::string& name) {
    return Transactions::exec("PermsQueries::roleExists", [&](pqxx::work& txn) -> bool {
        const auto res = txn.exec(pqxx::prepped{"role_exists"}, pqxx::params{name});
        return !res.empty() && res[0]["exists"].as<bool>();
    });
}

std::vector<std::shared_ptr<Role>> PermsQueries::listRoles(ListQueryParams&& params) {
    return Transactions::exec("PermsQueries::listRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
            "p.permissions::int AS permissions "
            "FROM role r "
            "JOIN permissions p ON r.id = p.role_id",
            params, "r.id", "r.name"
        );
        return roles_from_pq_res(txn.exec(sql));
    });
}

std::vector<std::shared_ptr<Role>> PermsQueries::listUserRoles(ListQueryParams&& params) {
    return Transactions::exec("PermsQueries::listUserRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
            "p.permissions::int AS permissions "
            "FROM role r "
            "JOIN permissions p ON r.id = p.role_id "
            "WHERE r.type = 'user'",
            params, "r.id", "r.name"
        );
        return roles_from_pq_res(txn.exec(sql));
    });
}

// #####################################################################################################
// ############################################# Vault Role  ###########################################
// #####################################################################################################

std::vector<std::shared_ptr<Role>> PermsQueries::listVaultRoles(ListQueryParams&& params) {
    return Transactions::exec("PermsQueries::listFSRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
            "p.permissions::int AS permissions "
            "FROM role r "
            "JOIN permissions p ON r.id = p.role_id "
            "WHERE r.type = 'vault'",
            params, "r.id", "r.name"
        );
        return roles_from_pq_res(txn.exec(sql));
    });
}

void PermsQueries::assignVaultRole(const std::shared_ptr<VaultRole>& roleAssignment) {
    Transactions::exec("PermsQueries::assignRole", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(roleAssignment->subject_type);
        p.append(roleAssignment->subject_id);
        p.append(roleAssignment->vault_id);
        p.append(roleAssignment->role_id);

        txn.exec(pqxx::prepped{"assign_vault_role"}, p);
    });
}

void PermsQueries::removeVaultRoleAssignment(const unsigned int id) {
    Transactions::exec("PermsQueries::removeAssignedRole", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_vault_role_assignment"}, pqxx::params{id});
    });
}

std::shared_ptr<VaultRole> PermsQueries::getVaultRoleBySubjectAndRoleId(const unsigned int subjectId, const std::string& subjectType, const unsigned int roleId) {
    return Transactions::exec("PermsQueries::getSubjectAssignedRole", [&](pqxx::work& txn) {
        pqxx::params role_params{subjectType, subjectId, roleId};
        const auto role = txn.exec(pqxx::prepped{"get_subject_assigned_vault_role"}, role_params).one_row();
        const auto overrides = txn.exec(pqxx::prepped{"list_vault_permission_overrides"}, pqxx::params{roleId});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::shared_ptr<VaultRole> PermsQueries::getVaultRoleBySubjectAndVaultId(const unsigned int subjectId, const std::string& subjectType, const unsigned int vaultId) {
    return Transactions::exec("PermsQueries::getSubjectAssignedRoleByVault", [&](pqxx::work& txn) {
        pqxx::params role_params{subjectType, subjectId, vaultId};
        const auto role = txn.exec(pqxx::prepped{"get_subject_assigned_vault_role_by_vault"}, role_params).one_row();
        const auto overrides = txn.exec(pqxx::prepped{"list_vault_permission_overrides"}, pqxx::params{role["role_id"].as<unsigned int>()});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::shared_ptr<VaultRole> PermsQueries::getVaultRole(const unsigned int id) {
    return Transactions::exec("PermsQueries::getAssignedRole", [&](pqxx::work& txn) {
        const auto role = txn.exec(pqxx::prepped{"get_vault_assigned_role"}, pqxx::params{id}).one_row();
        const auto overrides = txn.exec(pqxx::prepped{"get_assigned_role_overrides"}, pqxx::params{id});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::vector<std::shared_ptr<VaultRole>> PermsQueries::listVaultAssignedRoles(const unsigned int vaultId) {
    return Transactions::exec("PermsQueries::listAssignedRoles", [&](pqxx::work& txn) {
        pqxx::params p{vaultId};
        const auto roles = txn.exec(pqxx::prepped{"get_vault_assigned_roles"}, p);
        const auto overrides = txn.exec(pqxx::prepped{"list_vault_permission_overrides"}, p);
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

        const auto res = txn.exec(pqxx::prepped{"insert_vault_permission_override"}, p);
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

        txn.exec(pqxx::prepped{"update_vault_permission_override"}, p);
    });
}

void PermsQueries::removeVPermOverride(const unsigned int permOverrideId) {
    Transactions::exec("PermsQueries::removeVPermOverride", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_vault_permission_override"}, pqxx::params{permOverrideId});
    });
}

std::vector<std::shared_ptr<PermissionOverride>> PermsQueries::listVPermOverrides(const unsigned int vaultId, ListQueryParams&& params) {
    return Transactions::exec("PermsQueries::listVPermOverrides", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
            "FROM permission p "
            "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
            "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
            "WHERE vra.vault_id = " + txn.quote(vaultId),
            params, "p.id"
        );
        return permissionOverridesFromPqRes(txn.exec(sql));
    });
}

std::vector<std::shared_ptr<PermissionOverride>> PermsQueries::listAssignedVRoleOverrides(const VPermOverrideQuery& query, ListQueryParams&& params) {
    return Transactions::exec("PermsQueries::listAssignedVRoleOverrides", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
            "FROM permission p "
            "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
            "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
            "WHERE vra.vault_id = " + txn.quote(query.vault_id)
            + " AND vra.subject_type = " + txn.quote(query.subject_type)
            + " AND vra.subject_id = " + txn.quote(query.subject_id),
            params, "p.id"
        );
        return permissionOverridesFromPqRes(txn.exec(sql));
    });
}

std::shared_ptr<PermissionOverride> PermsQueries::getVPermOverride(const VPermOverrideQuery& query) {
    return Transactions::exec("PermsQueries::getVPermOverride", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(query.vault_id);
        p.append(query.subject_type);
        p.append(query.subject_id);
        p.append(query.bit_position);

        const auto res = txn.exec(pqxx::prepped{"get_permission_override_by_vault_subject_and_bitpos"}, p);
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
