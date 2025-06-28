#pragma once

#include <memory>
#include <vector>
#include <string>

namespace vh::types {
struct Role;
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

    // Role assignments (user/group)
    static void assignUserRole(const std::shared_ptr<types::Role>& roleAssignment);
    static void removeUserRole(unsigned int userId, unsigned int roleId);
    static std::shared_ptr<types::Role> getUserRole(unsigned int userId, unsigned int roleId);
    static std::vector<std::shared_ptr<types::Role>> listUserRoles(unsigned int userId);
    static std::vector<std::shared_ptr<types::Role>> listUserRolesByScope(const std::string& scope, unsigned int scopeId);

    // Permission queries
    static std::shared_ptr<types::Permission> getPermission(unsigned int id);
    static std::shared_ptr<types::Permission> getPermissionByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Permission>> listPermissions();
    static unsigned short countPermissions();
};

} // namespace vh::database
