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
    explicit User(std::string name, std::string email = "", bool isActive = true);
    explicit User(const pqxx::row& row);
    User(const pqxx::row& user, const pqxx::result& roles, const pqxx::result& overrides);

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
    [[nodiscard]] bool canCreateLocalVault() const;
    [[nodiscard]] bool canCreateCloudVault() const;
    [[nodiscard]] bool canDeleteVault() const;
    [[nodiscard]] bool canManageVaultSettings() const;
    [[nodiscard]] bool canManageVaultRoles() const;
    [[nodiscard]] bool canMigrateVaultData() const;
    [[nodiscard]] bool canManageAllVaults() const;

    // Vault permissions
    [[nodiscard]] bool canUploadFile(unsigned int vaultId) const;
    [[nodiscard]] bool canDownloadFile(unsigned int vaultId) const;
    [[nodiscard]] bool canDeleteFile(unsigned int vaultId) const;
    [[nodiscard]] bool canShareFilePublicly(unsigned int vaultId) const;
    [[nodiscard]] bool canShareFileInternally(unsigned int vaultId) const;
    [[nodiscard]] bool canLockFile(unsigned int vaultId) const;
    [[nodiscard]] bool canRenameFile(unsigned int vaultId) const;
    [[nodiscard]] bool canMoveFile(unsigned int vaultId) const;
    [[nodiscard]] bool canSyncFileLocally(unsigned int vaultId) const;
    [[nodiscard]] bool canSyncFileWithCloud(unsigned int vaultId) const;
    [[nodiscard]] bool canManageFileMetadata(unsigned int vaultId) const;
    [[nodiscard]] bool canChangeFileIcons(unsigned int vaultId) const;
    [[nodiscard]] bool canManageVersions(unsigned int vaultId) const;
    [[nodiscard]] bool canManageFileTags(unsigned int vaultId) const;

    [[nodiscard]] bool canUploadDirectory(unsigned int vaultId) const;
    [[nodiscard]] bool canDownloadDirectory(unsigned int vaultId) const;
    [[nodiscard]] bool canDeleteDirectory(unsigned int vaultId) const;
    [[nodiscard]] bool canShareDirPublicly(unsigned int vaultId) const;
    [[nodiscard]] bool canShareDirInternally(unsigned int vaultId) const;
    [[nodiscard]] bool canLockDirectory(unsigned int vaultId) const;
    [[nodiscard]] bool canRenameDirectory(unsigned int vaultId) const;
    [[nodiscard]] bool canMoveDirectory(unsigned int vaultId) const;
    [[nodiscard]] bool canSyncDirectoryLocally(unsigned int vaultId) const;
    [[nodiscard]] bool canSyncDirectoryWithCloud(unsigned int vaultId) const;
    [[nodiscard]] bool canManageDirectoryMetadata(unsigned int vaultId) const;
    [[nodiscard]] bool canChangeDirectoryIcons(unsigned int vaultId) const;
    [[nodiscard]] bool canManageDirectoryTags(unsigned int vaultId) const;
    [[nodiscard]] bool canListDirectory(unsigned int vaultId) const;
};

} // namespace vh::types

namespace vh::types {

void to_json(nlohmann::json& j, const User& u);
void from_json(const nlohmann::json& j, User& u);

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users);
nlohmann::json to_json(const std::shared_ptr<User>& user);

} // namespace vh::types
