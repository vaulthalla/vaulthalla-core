#include "database/Queries/PermsQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Role.hpp"
#include "types/AssignedRole.hpp"
#include "types/Permission.hpp"
#include "util/timestamp.hpp"

using namespace vh::database;
using namespace vh::types;

void PermsQueries::addRole(const std::shared_ptr<Role>& role) {
    Transactions::exec("PermsQueries::addRole", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(role->name);
        p.append(role->description);
        p.append(role->simplePermissions);

        txn.exec_prepared("insert_role", p);
    });
}

void PermsQueries::deleteRole(const unsigned int id) {
    Transactions::exec("PermsQueries::deleteRole", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(id);

        txn.exec_prepared("delete_role", p);
    });
}

void PermsQueries::updateRole(const std::shared_ptr<Role>& role) {
    Transactions::exec("PermsQueries::updateRole", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(role->id);
        p.append(role->name);
        p.append(role->description);
        p.append(role->simplePermissions);

        txn.exec_prepared("update_role", p);

        const auto existingIsSimple = txn.exec_prepared("get_permissions_type", role->id).one_row()[0].as<bool>();

        if (role->simplePermissions != existingIsSimple) {
            if (existingIsSimple) txn.exec_prepared("delete_simple_permissions", role->id);
            else txn.exec_prepared("delete_permissions", role->id);

            if (role->simplePermissions) {
                pqxx::params pSimple;
                pSimple.append(role->id);
                pSimple.append(bitStringFromMask(role->file_permissions));
                txn.exec_prepared("insert_simple_permissions", pSimple);
            } else {
                pqxx::params pPerms;
                pPerms.append(role->id);
                pPerms.append(bitStringFromMask(role->file_permissions));
                pPerms.append(bitStringFromMask(role->directory_permissions));
                txn.exec_prepared("insert_permissions", pPerms);
            }
        } else {
            if (role->simplePermissions) {
                pqxx::params pSimple;
                pSimple.append(role->id);
                pSimple.append(bitStringFromMask(role->file_permissions));
                txn.exec_prepared("update_simple_permissions", pSimple);
            } else {
                pqxx::params pPerms;
                pPerms.append(role->id);
                pPerms.append(bitStringFromMask(role->file_permissions));
                pPerms.append(bitStringFromMask(role->directory_permissions));
                txn.exec_prepared("update_permissions", pPerms);
            }
        }
    });
}

static std::shared_ptr<Role> makeRoleFromRow(const pqxx::row& row) {
    return std::make_shared<Role>(row);
}

std::shared_ptr<Role> PermsQueries::getRole(const unsigned int id) {
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


std::vector<std::shared_ptr<Role>> PermsQueries::listRoles() {
    return Transactions::exec("PermsQueries::listRoles", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM role");
        std::vector<std::shared_ptr<Role>> out;
        for (const auto& row : res) out.push_back(makeRoleFromRow(row));
        return out;
    });
}

void PermsQueries::assignRole(const std::shared_ptr<AssignedRole>& roleAssignment) {
    Transactions::exec("PermsQueries::assignRole", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(roleAssignment->subject_type);
        p.append(roleAssignment->subject_id);
        p.append(roleAssignment->role_id);
        p.append(roleAssignment->assigned_at);

        txn.exec_prepared("assign_role", p);
    });
}

void PermsQueries::removeAssignedRole(const unsigned int id) {
    Transactions::exec("PermsQueries::removeAssignedRole", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_assigned_role", pqxx::params{id});
    });
}

std::shared_ptr<AssignedRole> PermsQueries::getSubjectAssignedRole(const unsigned int subjectId, const std::string& subjectType, const unsigned int roleId) {
    return Transactions::exec("PermsQueries::getSubjectAssignedRole", [&](pqxx::work& txn) {
        pqxx::params role_params{subjectType, subjectId, roleId};
        const auto role = txn.exec_prepared("get_subject_assigned_role", role_params).one_row();
        const auto overrides = txn.exec_prepared("get_assigned_role_overrides", pqxx::params{roleId});
        return std::make_shared<AssignedRole>(role, overrides);
    });
}

std::shared_ptr<AssignedRole> PermsQueries::getAssignedRole(const unsigned int id) {
    return Transactions::exec("PermsQueries::getAssignedRole", [&](pqxx::work& txn) {
        const auto role = txn.exec_prepared("get_assigned_role", pqxx::params{id}).one_row();
        const auto overrides = txn.exec_prepared("get_assigned_role_overrides", pqxx::params{id});
        return std::make_shared<AssignedRole>(role, overrides);
    });
}

std::vector<std::shared_ptr<AssignedRole>> PermsQueries::listAssignedRoles(const unsigned int vaultId) {
    return Transactions::exec("PermsQueries::listAssignedRoles", [&](pqxx::work& txn) {
        pqxx::params p{vaultId};
        const auto roles = txn.exec_prepared("get_vault_assigned_roles", p);
        const auto overrides = txn.exec_prepared("get_vault_permissions_overrides", p);
        return assigned_roles_from_pq_result(roles, overrides);
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
