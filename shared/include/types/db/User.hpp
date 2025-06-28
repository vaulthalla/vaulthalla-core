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

struct AssignedRole;

struct User {
    unsigned short id, uid{};
    std::string name;
    std::string email;
    std::string password_hash;
    std::time_t created_at;
    std::optional<std::time_t> last_login;
    bool is_active;
    std::shared_ptr<AssignedRole> global_role; // Global role
    std::optional<std::vector<std::shared_ptr<AssignedRole>>> scoped_roles;  // Scoped roles, if any

    User();
    User(std::string name, std::string email, bool isActive);
    explicit User(const pqxx::row& row);
    User(const pqxx::row& user, const pqxx::result& roles);

    void updateUser(const nlohmann::json& j);
    void setPasswordHash(const std::string& hash);

    [[nodiscard]] bool isAdmin() const;
    [[nodiscard]] bool isSuperAdmin() const;

    // Admin checks
    [[nodiscard]] bool canCreateUser() const;
    [[nodiscard]] bool canCreateAdminUser() const;
    [[nodiscard]] bool canDeactivateUser() const;
    [[nodiscard]] bool canResetUserPassword() const;
    [[nodiscard]] bool canManageRoles() const;
    [[nodiscard]] bool canManageSettings() const;
    [[nodiscard]] bool canViewAuditLog() const;
    [[nodiscard]] bool canManageAPIKeys() const;

    // Vault checks
    [[nodiscard]] bool canCreateLocalVault() const;
    [[nodiscard]] bool canCreateCloudVault() const;
    [[nodiscard]] bool canDeleteVault() const;
    [[nodiscard]] bool canAdjustVaultSettings() const;
    [[nodiscard]] bool canMigrateVaultData() const;
    [[nodiscard]] bool canCreateVolume() const;
    [[nodiscard]] bool canDeleteVolume() const;
    [[nodiscard]] bool canResizeVolume() const;
    [[nodiscard]] bool canMoveVolume() const;
    [[nodiscard]] bool canAssignVolumeToGroup() const;

    // File checks
    [[nodiscard]] bool canUploadFile() const;
    [[nodiscard]] bool canDownloadFile() const;
    [[nodiscard]] bool canDeleteFile() const;
    [[nodiscard]] bool canShareFilePublicly() const;
    [[nodiscard]] bool canShareFileWithGroup() const;
    [[nodiscard]] bool canLockFile() const;
    [[nodiscard]] bool canRenameFile() const;
    [[nodiscard]] bool canMoveFile() const;

    // Directory checks
    [[nodiscard]] bool canCreateDirectory() const;
    [[nodiscard]] bool canDeleteDirectory() const;
    [[nodiscard]] bool canRenameDirectory() const;
    [[nodiscard]] bool canMoveDirectory() const;
    [[nodiscard]] bool canListDirectory() const;
};

} // namespace vh::types

namespace vh::types {

void to_json(nlohmann::json& j, const User& u);
void from_json(const nlohmann::json& j, User& u);

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users);
nlohmann::json to_json(const std::shared_ptr<User>& user);

} // namespace vh::types
