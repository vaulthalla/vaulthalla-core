#pragma once

#include <memory>
#include <vector>

namespace vh::types {
struct BaseRole;
struct Role;
struct Permission;
}

namespace vh::database {

struct PermsQueries {
    static void addRole(const std::shared_ptr<types::BaseRole>& role);
    static void deleteRole(unsigned int id);
    static void updateRole(const std::shared_ptr<types::BaseRole>& role);
    static std::shared_ptr<types::BaseRole> getRole(unsigned int id);
    static std::shared_ptr<types::BaseRole> getRoleByName(const std::string& name);
    static std::vector<std::shared_ptr<types::BaseRole>> listRoles();

    static void assignUserRole(const std::shared_ptr<types::Role>& userRole);
    static void removeUserRole(unsigned int userId, unsigned int roleId);
    static std::shared_ptr<types::Role> getUserRole(unsigned int userId, unsigned int roleId);
    static std::vector<std::shared_ptr<types::Role>> listUserRoles(unsigned int userId);
    static std::vector<std::shared_ptr<types::Role>> listUserRolesByScope(const std::string& scope, unsigned int scopeId);

    static std::shared_ptr<types::Permission> getPermission(unsigned int id);
    static std::shared_ptr<types::Permission> getPermissionByName(const std::string& name);
    static std::vector<std::shared_ptr<types::Permission>> listPermissions();
    static unsigned short countPermissions();
};

}
