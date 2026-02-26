#include "db/query/rbac/Permission.hpp"
#include "db/Transactions.hpp"
#include "rbac/model/Role.hpp"
#include "rbac/model/VaultRole.hpp"
#include "rbac/model/Permission.hpp"
#include "rbac/model/PermissionOverride.hpp"
#include "log/Registry.hpp"

using namespace vh::db::model;

namespace vh::db::query::rbac {
// #################################################################################################
// ############################################ Role  #############################################
// #################################################################################################

unsigned int Permission::addRole(const RolePtr& role) {
    return Transactions::exec("Permission::addRole", [&](pqxx::work& txn) {
        const auto role_id = txn.exec(pqxx::prepped{"insert_role"},
            pqxx::params{role->name, role->description, role->type}).one_field().as<unsigned int>();
        txn.exec(pqxx::prepped("insert_role_permission"), pqxx::params{role_id, vh::rbac::model::bitStringFromMask(role->permissions)});
        return role_id;
    });
}

void Permission::deleteRole(const unsigned int id) {
    Transactions::exec("Permission::deleteRole", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_role"}, pqxx::params{id});
    });
}

void Permission::updateRole(const RolePtr& role) {
    Transactions::exec("Permission::updateRole", [&](pqxx::work& txn) {
        pqxx::params p{role->id, role->name, role->description, role->type};
        txn.exec(pqxx::prepped{"update_role"}, p);

        log::Registry::db()->trace("[Permission] Updating permissions for role ID {} to mask {}",
            role->id, vh::rbac::model::bitStringFromMask(role->permissions));

        const pqxx::params perms_params{role->id, vh::rbac::model::bitStringFromMask(role->permissions)};
        txn.exec(pqxx::prepped{"update_role_permissions"}, perms_params);
    });
}

Permission::RolePtr Permission::getRole(const unsigned int id) {
    return Transactions::exec("Permission::getRole", [&](pqxx::work& txn) -> RolePtr {
        const auto res = txn.exec(pqxx::prepped{"get_role"}, pqxx::params{id});
        if (res.empty()) return nullptr;
        return std::make_shared<Role>(res.one_row());
    });
}

Permission::RolePtr Permission::getRoleByName(const std::string& name) {
    return Transactions::exec("Permission::getRoleByName", [&](pqxx::work& txn) -> RolePtr {
        const auto res = txn.exec(pqxx::prepped{"get_role_by_name"}, pqxx::params{name});
        if (res.empty()) return nullptr;
        return std::make_shared<Role>(res.one_row());
    });
}

bool Permission::roleExists(const std::string& name) {
    return Transactions::exec("Permission::roleExists", [&](pqxx::work& txn) -> bool {
        const auto res = txn.exec(pqxx::prepped{"role_exists"}, pqxx::params{name});
        return !res.empty() && res[0]["exists"].as<bool>();
    });
}

std::vector<Permission::RolePtr> Permission::listRoles(ListQueryParams&& params) {
    return Transactions::exec("Permission::listRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
            "p.permissions::int AS permissions "
            "FROM role r "
            "JOIN permissions p ON r.id = p.role_id",
            params, "r.id", "r.name"
        );
        return vh::rbac::model::roles_from_pq_res(txn.exec(sql));
    });
}

std::vector<Permission::RolePtr> Permission::listUserRoles(ListQueryParams&& params) {
    return Transactions::exec("Permission::listUserRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
            "p.permissions::int AS permissions "
            "FROM role r "
            "JOIN permissions p ON r.id = p.role_id "
            "WHERE r.type = 'user'",
            params, "r.id", "r.name"
        );
        return vh::rbac::model::roles_from_pq_res(txn.exec(sql));
    });
}

// #####################################################################################################
// ############################################# Vault Role  ###########################################
// #####################################################################################################

std::vector<Permission::RolePtr> Permission::listVaultRoles(ListQueryParams&& params) {
    return Transactions::exec("Permission::listFSRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
            "p.permissions::int AS permissions "
            "FROM role r "
            "JOIN permissions p ON r.id = p.role_id "
            "WHERE r.type = 'vault'",
            params, "r.id", "r.name"
        );
        return vh::rbac::model::roles_from_pq_res(txn.exec(sql));
    });
}

void Permission::assignVaultRole(const std::shared_ptr<VaultRole>& roleAssignment) {
    Transactions::exec("Permission::assignRole", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(roleAssignment->subject_type);
        p.append(roleAssignment->subject_id);
        p.append(roleAssignment->vault_id);
        p.append(roleAssignment->role_id);

        roleAssignment->id = txn.exec(pqxx::prepped{"assign_vault_role"}, p).one_field().as<unsigned int>();

        if (!roleAssignment->permission_overrides.empty()) {
            for (const auto& override : roleAssignment->permission_overrides) {
                override->assignment_id = roleAssignment->id;
                pqxx::params override_params{
                    override->assignment_id,
                    override->permission.id,
                    override->patternStr,
                    override->enabled,
                    to_string(override->effect)
                };
                const auto res = txn.exec(pqxx::prepped{"insert_vault_permission_override"}, override_params);
            }
        }
    });
}

void Permission::removeVaultRoleAssignment(const unsigned int id) {
    Transactions::exec("Permission::removeAssignedRole", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_vault_role_assignment"}, pqxx::params{id});
    });
}

std::shared_ptr<Permission::VaultRole> Permission::getVaultRoleBySubjectAndRoleId(const unsigned int subjectId, const std::string& subjectType, const unsigned int roleId) {
    return Transactions::exec("Permission::getSubjectAssignedRole", [&](pqxx::work& txn) {
        pqxx::params role_params{subjectType, subjectId, roleId};
        const auto role = txn.exec(pqxx::prepped{"get_subject_assigned_vault_role"}, role_params).one_row();
        const auto overrides = txn.exec(pqxx::prepped{"list_vault_permission_overrides"}, pqxx::params{roleId});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::shared_ptr<Permission::VaultRole> Permission::getVaultRoleBySubjectAndVaultId(const unsigned int subjectId, const std::string& subjectType, const unsigned int vaultId) {
    return Transactions::exec("Permission::getSubjectAssignedRoleByVault", [&](pqxx::work& txn) {
        pqxx::params role_params{subjectType, subjectId, vaultId};
        const auto role = txn.exec(pqxx::prepped{"get_subject_assigned_vault_role_by_vault"}, role_params).one_row();
        const auto overrides = txn.exec(pqxx::prepped{"list_vault_permission_overrides"}, pqxx::params{role["role_id"].as<unsigned int>()});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::shared_ptr<Permission::VaultRole> Permission::getVaultRole(const unsigned int id) {
    return Transactions::exec("Permission::getAssignedRole", [&](pqxx::work& txn) {
        const auto role = txn.exec(pqxx::prepped{"get_vault_assigned_role"}, pqxx::params{id}).one_row();
        const auto overrides = txn.exec(pqxx::prepped{"get_assigned_role_overrides"}, pqxx::params{id});
        return std::make_shared<VaultRole>(role, overrides);
    });
}

std::vector<std::shared_ptr<Permission::VaultRole>> Permission::listVaultAssignedRoles(const unsigned int vaultId) {
    return Transactions::exec("Permission::listAssignedRoles", [&](pqxx::work& txn) {
        pqxx::params p{vaultId};
        const auto roles = txn.exec(pqxx::prepped{"get_vault_assigned_roles"}, p);
        const auto overrides = txn.exec(pqxx::prepped{"list_vault_permission_overrides"}, p);
        return vh::rbac::model::vault_roles_vector_from_pq_result(roles, overrides);
    });
}

// #####################################################################################################
// ##################################### Vault Permission Override  ####################################
// #####################################################################################################

unsigned int Permission::addVPermOverride(const OverridePtr& override) {
    return Transactions::exec("Permission::addVPermOverride", [&](pqxx::work& txn) {
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

void Permission::updateVPermOverride(const OverridePtr& override) {
    Transactions::exec("Permission::updateVPermOverride", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(override->id);
        p.append(override->patternStr);
        p.append(override->enabled);
        p.append(to_string(override->effect));

        txn.exec(pqxx::prepped{"update_vault_permission_override"}, p);
    });
}

void Permission::removeVPermOverride(const unsigned int permOverrideId) {
    Transactions::exec("Permission::removeVPermOverride", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_vault_permission_override"}, pqxx::params{permOverrideId});
    });
}

std::vector<Permission::OverridePtr> Permission::listVPermOverrides(const unsigned int vaultId, ListQueryParams&& params) {
    return Transactions::exec("Permission::listVPermOverrides", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
            "FROM permission p "
            "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
            "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
            "WHERE vra.vault_id = " + txn.quote(vaultId),
            params, "p.id"
        );
        return vh::rbac::model::permissionOverridesFromPqRes(txn.exec(sql));
    });
}

std::vector<Permission::OverridePtr> Permission::listAssignedVRoleOverrides(const VPermOverrideQuery& query, ListQueryParams&& params) {
    return Transactions::exec("Permission::listAssignedVRoleOverrides", [&](pqxx::work& txn) {
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
        return vh::rbac::model::permissionOverridesFromPqRes(txn.exec(sql));
    });
}

Permission::OverridePtr Permission::getVPermOverride(const VPermOverrideQuery& query) {
    return Transactions::exec("Permission::getVPermOverride", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(query.vault_id);
        p.append(query.subject_type);
        p.append(query.subject_id);
        p.append(query.bit_position);

        const auto res = txn.exec(pqxx::prepped{"get_permission_override_by_vault_subject_and_bitpos"}, p);
        if (res.empty()) return OverridePtr(nullptr);
        return std::make_shared<Override>(res.one_row());
    });
}


// #################################################################################################
// ########################################## Permission  ##########################################
// #################################################################################################

Permission::PermPtr Permission::getPermission(const unsigned int id) {
    return Transactions::exec("Permission::getPermission", [&](pqxx::work& txn) {
        return std::make_shared<Perm>(txn.exec("SELECT * FROM permission WHERE id = " + txn.quote(id)).one_row());
    });
}

Permission::PermPtr Permission::getPermissionByName(const std::string& name) {
    return Transactions::exec("Permission::getPermissionByName", [&](pqxx::work& txn) {
        return std::make_shared<Perm>(txn.exec("SELECT * FROM permission WHERE name = " + txn.quote(name)).one_row());
    });
}

std::vector<Permission::PermPtr> Permission::listPermissions() {
    return Transactions::exec("Permission::listPermissions", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM permissions");
        std::vector<PermPtr> out;
        for (const auto& row : res) out.push_back(std::make_shared<Perm>(row));
        return out;
    });
}

unsigned short Permission::countPermissions() {
    return Transactions::exec("Permission::countPermissions", [&](pqxx::work& txn) {
        return txn.exec("SELECT COUNT(*) FROM permissions")[0][0].as<unsigned short>();
    });
}

}
