#pragma once

#include <memory>
#include <vector>
#include <string>

namespace vh::types {
struct Role;
struct UserRole;
struct VaultRole;
struct Permission;
}

namespace vh::database {

struct PermsQueries {
    // Role CRUD
    static void addRole(const std::shared_ptr<types::Role>& role);
    static void deleteRole(unsigned int id);
    static void updateRole(const std::shared_ptr<types::Role>& role);
    static std::shared_ptr<types::Role> getRole(unsigned int id);
    static std::shared_ptr<types::Role> getRoleByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Role>> listRoles();
    static std::vector<std::shared_ptr<types::Role>> listUserRoles();
    static std::vector<std::shared_ptr<types::Role>> listVaultRoles();

    // Vault Role CRUD
    static void assignVaultRole(const std::shared_ptr<types::VaultRole>& roleAssignment);
    static void removeVaultRoleAssignment(unsigned int id);
    static std::shared_ptr<types::VaultRole> getVaultRoleBySubject(unsigned int subjectId, const std::string& subjectType, unsigned int roleId);
    static std::shared_ptr<types::VaultRole> getVaultRole(unsigned int id);
    static std::vector<std::shared_ptr<types::VaultRole>> listVaultAssignedRoles(unsigned int vaultId);

    // Permission queries
    static std::shared_ptr<types::Permission> getPermission(unsigned int id);
    static std::shared_ptr<types::Permission> getPermissionByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Permission>> listPermissions();
    static unsigned short countPermissions();
};

} // namespace vh::database
