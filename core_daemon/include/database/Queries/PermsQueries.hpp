#pragma once

#include <memory>
#include <vector>

namespace vh::types {
struct Role;
struct UserRole;
struct Permission;
}

namespace vh::database {

struct PermsQueries {
    static void addRole(const std::shared_ptr<types::Role>& role);
    static void deleteRole(unsigned int id);
    static void updateRole(const std::shared_ptr<types::Role>& role);
    static std::shared_ptr<types::Role> getRole(unsigned int id);
    static std::shared_ptr<types::Role> getRoleByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Role>> listRoles();

    static void assignUserRole(const std::shared_ptr<types::UserRole>& userRole);
    static void removeUserRole(unsigned int userId, unsigned int roleId);
    static std::shared_ptr<types::UserRole> getUserRole(unsigned int userId, unsigned int roleId);
    static std::vector<std::shared_ptr<types::UserRole>> listUserRoles(unsigned int userId);
    static std::vector<std::shared_ptr<types::UserRole>> listUserRolesByScope(const std::string& scope, unsigned int scopeId);

    static std::shared_ptr<types::Permission> getPermission(unsigned int id);
    static std::shared_ptr<types::Permission> getPermissionByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Permission>> listPermissions();
    static unsigned short countPermissions();
};

}
