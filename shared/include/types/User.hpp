#pragma once

#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <ctime>
#include <cstdint>

#include <nlohmann/json_fwd.hpp> // Forward-decl only

#include "Permission.hpp"

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct AssignedRole;

struct User {
    static constexpr uint16_t ADMIN_MASK = 0x00FD;

    unsigned short id{};
    std::string name, password_hash;
    std::optional<std::string> email{std::nullopt};
    uint16_t permissions = 0; // Bitmask of permissions
    std::time_t created_at{};
    std::optional<std::time_t> last_login;
    bool is_active{true};
    std::vector<std::shared_ptr<AssignedRole>> roles;

    User();
    User(std::string name, std::string email = "");
    explicit User(const pqxx::row& row);
    User(const pqxx::row& user, const pqxx::result& roles);

    [[nodiscard]] std::shared_ptr<AssignedRole> getRole(unsigned int vaultId) const;

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
