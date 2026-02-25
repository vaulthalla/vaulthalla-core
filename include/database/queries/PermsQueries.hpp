#pragma once

#include "database/model/ListQueryParams.hpp"

#include <memory>
#include <vector>
#include <string>

namespace vh::rbac::model {
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
    static unsigned int addRole(const std::shared_ptr<rbac::model::Role>& role);
    static void deleteRole(unsigned int id);
    static void updateRole(const std::shared_ptr<rbac::model::Role>& role);
    static std::shared_ptr<rbac::model::Role> getRole(unsigned int id);
    static std::shared_ptr<rbac::model::Role> getRoleByName(const std::string& name);
    static std::vector<std::shared_ptr<rbac::model::Role>> listRoles(model::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<rbac::model::Role>> listUserRoles(model::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<rbac::model::Role>> listVaultRoles(model::ListQueryParams&& params = {});
    [[nodiscard]] static bool roleExists(const std::string& name);

    // Vault Role CRUD
    static void assignVaultRole(const std::shared_ptr<rbac::model::VaultRole>& roleAssignment);
    static void removeVaultRoleAssignment(unsigned int id);
    static std::shared_ptr<rbac::model::VaultRole> getVaultRoleBySubjectAndRoleId(unsigned int subjectId, const std::string& subjectType, unsigned int roleId);
    static std::shared_ptr<rbac::model::VaultRole> getVaultRoleBySubjectAndVaultId(unsigned int subjectId, const std::string& subjectType, unsigned int vaultId);
    static std::shared_ptr<rbac::model::VaultRole> getVaultRole(unsigned int id);
    static std::vector<std::shared_ptr<rbac::model::VaultRole>> listVaultAssignedRoles(unsigned int vaultId);

    // Vault Permission Overrides
    static unsigned int addVPermOverride(const std::shared_ptr<rbac::model::PermissionOverride>& override);
    static void updateVPermOverride(const std::shared_ptr<rbac::model::PermissionOverride>& override);
    static void removeVPermOverride(unsigned int permOverrideId);
    static std::vector<std::shared_ptr<rbac::model::PermissionOverride>> listVPermOverrides(unsigned int vaultId, model::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<rbac::model::PermissionOverride>> listAssignedVRoleOverrides(const VPermOverrideQuery& query, model::ListQueryParams&& params = {});
    static std::shared_ptr<rbac::model::PermissionOverride> getVPermOverride(const VPermOverrideQuery& query);

    // Permission queries
    static std::shared_ptr<rbac::model::Permission> getPermission(unsigned int id);
    static std::shared_ptr<rbac::model::Permission> getPermissionByName(const std::string& name);
    static std::shared_ptr<rbac::model::VaultPermission> getVaultPermission(const rbac::model::VaultPermission& vp);
    static std::vector<std::shared_ptr<rbac::model::Permission>> listPermissions();
    static unsigned short countPermissions();
};

} // namespace vh::database
