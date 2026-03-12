#pragma once

#include "db/model/ListQueryParams.hpp"

#include <memory>
#include <vector>
#include <string>

namespace vh::rbac::model {
struct Role;
struct Admin;
struct Vault;
struct Permission;
struct Override;
enum class VaultPermission : uint16_t;
}

namespace vh::db::query::rbac {

class Permission {
    using Perm = vh::rbac::model::Permission;
    using Role = vh::rbac::model::Role;
    using UserRole = vh::rbac::model::Admin;
    using VaultRole = vh::rbac::model::Vault;

    using PermPtr = std::shared_ptr<Perm>;
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


    // Permission queries
    static PermPtr getPermission(unsigned int id);
    static PermPtr getPermissionByName(const std::string& name);
    static std::shared_ptr<vh::rbac::model::VaultPermission> getVaultPermission(const vh::rbac::model::VaultPermission& vp);
    static std::vector<PermPtr> listPermissions();
    static unsigned short countPermissions();
};

}
