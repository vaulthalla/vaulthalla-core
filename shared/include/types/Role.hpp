#pragma once

#include "types/Permission.hpp"

#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct Role {
    unsigned int id;
    std::string name, description;
    std::time_t created_at;
    uint16_t file_permissions, directory_permissions;

    Role() = default;
    explicit Role(const pqxx::row& row);
    explicit Role(const nlohmann::json& j);

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

    [[nodiscard]] bool canUploadFile() const;
    [[nodiscard]] bool canDownloadFile() const;
    [[nodiscard]] bool canDeleteFile() const;
    [[nodiscard]] bool canShareFilePublicly() const;
    [[nodiscard]] bool canShareFileWithGroup() const;
    [[nodiscard]] bool canLockFile() const;
    [[nodiscard]] bool canRenameFile() const;
    [[nodiscard]] bool canMoveFile() const;

    [[nodiscard]] bool canCreateDirectory() const;
    [[nodiscard]] bool canDeleteDirectory() const;
    [[nodiscard]] bool canRenameDirectory() const;
    [[nodiscard]] bool canMoveDirectory() const;
    [[nodiscard]] bool canListDirectory() const;
};

// JSON + DB helpers
void to_json(nlohmann::json& j, const Role& r);
void from_json(const nlohmann::json& j, Role& r);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& roles);

std::vector<std::shared_ptr<Role>> roles_from_pq_res(const pqxx::result& res);

} // namespace vh::types
