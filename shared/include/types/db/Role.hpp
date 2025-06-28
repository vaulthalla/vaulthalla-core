#pragma once

#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>
#include <optional>

namespace pqxx {
    class row;
}

namespace vh::types {

struct Role {
    unsigned int id;                  // Role ID
    unsigned int role_id;              // Original role template (if you're doing templated roles)
    unsigned int subject_id;           // Who this role is assigned to (user/group)
    std::string name, display_name, description;
    std::string scope;                 // e.g. "global", "vault", "volume"
    std::optional<unsigned int> scope_id;
    uint16_t admin_permissions = 0;
    uint16_t vault_permissions = 0;
    uint16_t file_permissions = 0;
    uint16_t directory_permissions = 0;
    std::time_t created_at;
    std::time_t assigned_at;
    bool inherited = false;

    Role() = default;
    explicit Role(const pqxx::row& row);
    explicit Role(const nlohmann::json& j);

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

// JSON + DB helpers
void to_json(nlohmann::json& j, const Role& r);
void from_json(const nlohmann::json& j, Role& r);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& roles);
std::vector<std::shared_ptr<Role>> roles_from_json(const nlohmann::json& j);

}
