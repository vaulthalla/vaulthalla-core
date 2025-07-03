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
    bool simplePermissions{false};
    uint16_t file_permissions{0}, directory_permissions{0};

    Role() = default;
    explicit Role(const pqxx::row& row);
    explicit Role(const nlohmann::json& j);

    [[nodiscard]] bool canUploadFile() const;
    [[nodiscard]] bool canDownloadFile() const;
    [[nodiscard]] bool canDeleteFile() const;
    [[nodiscard]] bool canShareFilePublicly() const;
    [[nodiscard]] bool canShareFileInternally() const;
    [[nodiscard]] bool canLockFile() const;
    [[nodiscard]] bool canRenameFile() const;
    [[nodiscard]] bool canMoveFile() const;
    [[nodiscard]] bool canSyncFileLocally() const;
    [[nodiscard]] bool canSyncFileWithCloud() const;
    [[nodiscard]] bool canManageFileMetadata() const;
    [[nodiscard]] bool canChangeFileIcons() const;
    [[nodiscard]] bool canManageVersions() const;
    [[nodiscard]] bool canManageFileTags() const;

    [[nodiscard]] bool canUploadDirectory() const;
    [[nodiscard]] bool canDownloadDirectory() const;
    [[nodiscard]] bool canDeleteDirectory() const;
    [[nodiscard]] bool canShareDirPublicly() const;
    [[nodiscard]] bool canShareDirInternally() const;
    [[nodiscard]] bool canLockDirectory() const;
    [[nodiscard]] bool canRenameDirectory() const;
    [[nodiscard]] bool canMoveDirectory() const;
    [[nodiscard]] bool canSyncDirectoryLocally() const;
    [[nodiscard]] bool canSyncDirectoryWithCloud() const;
    [[nodiscard]] bool canManageDirectoryMetadata() const;
    [[nodiscard]] bool canChangeDirectoryIcons() const;
    [[nodiscard]] bool canManageDirectoryTags() const;
    [[nodiscard]] bool canListDirectory() const;
};

// JSON + DB helpers
void to_json(nlohmann::json& j, const Role& r);
void from_json(const nlohmann::json& j, Role& r);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& roles);

std::vector<std::shared_ptr<Role>> roles_from_pq_res(const pqxx::result& res);

} // namespace vh::types
