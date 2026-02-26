#pragma once

#include "db/model/ListQueryParams.hpp"

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

namespace vh::db::query::rbac {

struct VPermOverrideQuery {
    unsigned int vault_id;
    std::string subject_type;
    unsigned int subject_id;
    unsigned int bit_position = 0;
};

class Permission {
    using Perm = vh::rbac::model::Permission;
    using Override = vh::rbac::model::PermissionOverride;
    using Role = vh::rbac::model::Role;
    using UserRole = vh::rbac::model::UserRole;
    using VaultRole = vh::rbac::model::VaultRole;

    using PermPtr = std::shared_ptr<Perm>;
    using OverridePtr = std::shared_ptr<Override>;
    using RolePtr = std::shared_ptr<Role>;
    using UserRolePtr = std::shared_ptr<UserRole>;
    using VaultRolePtr = std::shared_ptr<VaultRole>;

public:
    // Role CRUD
    static unsigned int addRole(const RolePtr& role);
    static void deleteRole(unsigned int id);
    static void updateRole(const RolePtr& role);
    static RolePtr getRole(unsigned int id);
    static RolePtr getRoleByName(const std::string& name);
    static std::vector<RolePtr> listRoles(model::ListQueryParams&& params = {});
    static std::vector<RolePtr> listUserRoles(model::ListQueryParams&& params = {});
    static std::vector<RolePtr> listVaultRoles(model::ListQueryParams&& params = {});
    [[nodiscard]] static bool roleExists(const std::string& name);

    // Vault Role CRUD
    static void assignVaultRole(const VaultRolePtr& roleAssignment);
    static void removeVaultRoleAssignment(unsigned int id);
    static VaultRolePtr getVaultRoleBySubjectAndRoleId(unsigned int subjectId, const std::string& subjectType, unsigned int roleId);
    static VaultRolePtr getVaultRoleBySubjectAndVaultId(unsigned int subjectId, const std::string& subjectType, unsigned int vaultId);
    static VaultRolePtr getVaultRole(unsigned int id);
    static std::vector<VaultRolePtr> listVaultAssignedRoles(unsigned int vaultId);

    // Vault Permission Overrides
    static unsigned int addVPermOverride(const OverridePtr& override);
    static void updateVPermOverride(const OverridePtr& override);
    static void removeVPermOverride(unsigned int permOverrideId);
    static std::vector<OverridePtr> listVPermOverrides(unsigned int vaultId, model::ListQueryParams&& params = {});
    static std::vector<OverridePtr> listAssignedVRoleOverrides(const VPermOverrideQuery& query, model::ListQueryParams&& params = {});
    static OverridePtr getVPermOverride(const VPermOverrideQuery& query);

    // Permission queries
    static PermPtr getPermission(unsigned int id);
    static PermPtr getPermissionByName(const std::string& name);
    static std::shared_ptr<vh::rbac::model::VaultPermission> getVaultPermission(const vh::rbac::model::VaultPermission& vp);
    static std::vector<PermPtr> listPermissions();
    static unsigned short countPermissions();
};

}
