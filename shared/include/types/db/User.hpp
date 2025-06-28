#pragma once

#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <ctime>

#include <nlohmann/json_fwd.hpp> // Forward-decl only

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct Role;

struct User {
    unsigned short id, uid{};
    std::string name;
    std::string email;
    std::string password_hash;
    std::time_t created_at;
    std::optional<std::time_t> last_login;
    bool is_active;
    std::shared_ptr<Role> global_role; // Global role
    std::optional<std::vector<std::shared_ptr<Role>>> scoped_roles;  // Scoped roles, if any

    User();
    User(std::string name, std::string email, const bool isActive);
    explicit User(const pqxx::row& row);
    User(const pqxx::row& user, const pqxx::result& roles);

    void updateUser(const nlohmann::json& j);
    void setPasswordHash(const std::string& hash);

    // Admin checks
    bool canCreateUser() const;
    bool canCreateAdminUser() const;
    bool canDeactivateUser() const;
    bool canResetUserPassword() const;
    bool canManageRoles() const;
    bool canManageSettings() const;
    bool canViewAuditLog() const;
    bool canManageAPIKeys() const;

    // Vault checks
    bool canCreateLocalVault() const;
    bool canCreateCloudVault() const;
    bool canDeleteVault() const;
    bool canAdjustVaultSettings() const;
    bool canMigrateVaultData() const;
    bool canCreateVolume() const;
    bool canDeleteVolume() const;
    bool canResizeVolume() const;
    bool canMoveVolume() const;
    bool canAssignVolumeToGroup() const;

    // File checks
    bool canUploadFile() const;
    bool canDownloadFile() const;
    bool canDeleteFile() const;
    bool canShareFilePublicly() const;
    bool canShareFileWithGroup() const;
    bool canLockFile() const;
    bool canRenameFile() const;
    bool canMoveFile() const;

    // Directory checks
    bool canCreateDirectory() const;
    bool canDeleteDirectory() const;
    bool canRenameDirectory() const;
    bool canMoveDirectory() const;
    bool canListDirectory() const;
};

} // namespace vh::types

namespace vh::types {

void to_json(nlohmann::json& j, const User& u);
void from_json(const nlohmann::json& j, User& u);

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users);
nlohmann::json to_json(const std::shared_ptr<User>& user);

} // namespace vh::types
