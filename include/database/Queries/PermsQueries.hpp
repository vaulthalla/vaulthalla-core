#pragma once

#include "types/admin/ListQueryParams.hpp"

#include <memory>
#include <vector>
#include <string>

namespace vh::types {
struct Role;
struct UserRole;
struct VaultRole;
struct Permission;
struct PermissionOverride;
enum class VaultPermission : uint16_t;
}

namespace vh::database {

struct VPermOverrideQuery {
    unsigned int vault_id;
    std::string subject_type;
    unsigned int subject_id;
    unsigned int bit_position = 0;
};

struct PermsQueries {
    // Role CRUD
    static unsigned int addRole(const std::shared_ptr<types::Role>& role);
    static void deleteRole(unsigned int id);
    static void updateRole(const std::shared_ptr<types::Role>& role);
    static std::shared_ptr<types::Role> getRole(unsigned int id);
    static std::shared_ptr<types::Role> getRoleByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Role>> listRoles(types::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<types::Role>> listUserRoles(types::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<types::Role>> listVaultRoles(types::ListQueryParams&& params = {});
    [[nodiscard]] static bool roleExists(const std::string& name);

    // Vault Role CRUD
    static void assignVaultRole(const std::shared_ptr<types::VaultRole>& roleAssignment);
    static void removeVaultRoleAssignment(unsigned int id);
    static std::shared_ptr<types::VaultRole> getVaultRoleBySubjectAndRoleId(unsigned int subjectId, const std::string& subjectType, unsigned int roleId);
    static std::shared_ptr<types::VaultRole> getVaultRoleBySubjectAndVaultId(unsigned int subjectId, const std::string& subjectType, unsigned int vaultId);
    static std::shared_ptr<types::VaultRole> getVaultRole(unsigned int id);
    static std::vector<std::shared_ptr<types::VaultRole>> listVaultAssignedRoles(unsigned int vaultId);

    // Vault Permission Overrides
    static unsigned int addVPermOverride(const std::shared_ptr<types::PermissionOverride>& override);
    static void updateVPermOverride(const std::shared_ptr<types::PermissionOverride>& override);
    static void removeVPermOverride(unsigned int permOverrideId);
    static std::vector<std::shared_ptr<types::PermissionOverride>> listVPermOverrides(unsigned int vaultId, types::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<types::PermissionOverride>> listAssignedVRoleOverrides(const VPermOverrideQuery& query, types::ListQueryParams&& params = {});
    static std::shared_ptr<types::PermissionOverride> getVPermOverride(const VPermOverrideQuery& query);

    // Permission queries
    static std::shared_ptr<types::Permission> getPermission(unsigned int id);
    static std::shared_ptr<types::Permission> getPermissionByName(const std::string& name);
    static std::shared_ptr<types::VaultPermission> getVaultPermission(const types::VaultPermission& vp);
    static std::vector<std::shared_ptr<types::Permission>> listPermissions();
    static unsigned short countPermissions();
};

} // namespace vh::database
