#pragma once

#include "Role.hpp"

#include <string>
#include <ctime>
#include <filesystem>
#include <optional>
#include <memory>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <pqxx/result.hxx>

#include "PermissionOverride.hpp"

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct PermissionOverride;

struct AssignedRole : Role {
    unsigned int id, subject_id, role_id, vault_id;
    std::string subject_type; // 'user' or 'group'
    std::time_t assigned_at;
    std::vector<std::shared_ptr<PermissionOverride> > permission_overrides;

    AssignedRole() = default;

    AssignedRole(const pqxx::row& row, const pqxx::result& overrides);

    AssignedRole(const pqxx::row& row, const std::vector<pqxx::row>& overrides);

    explicit AssignedRole(const nlohmann::json& j);

    std::vector<std::shared_ptr<PermissionOverride> > getPermissionOverrides(unsigned short bit, bool isFile) const;

    template <typename T> bool validatePermission(const uint16_t mask, T perm,
                                                  const std::filesystem::path& path = {},
                                                  const bool isFile = true) const {
        const bool isEnabled = (mask & static_cast<uint16_t>(perm)) != 0;
        if (path.empty()) return isEnabled;

        const auto pathStr = path.string();
        const auto bit = fsPermToBit(perm);
        auto overrides = getPermissionOverrides(bit, isFile);
        if (overrides.empty()) return isEnabled;

        // Sort: best match first
        std::sort(overrides.begin(), overrides.end(),
                  [&pathStr](const std::shared_ptr<PermissionOverride>& a, const std::shared_ptr<PermissionOverride>& b) {
                      std::smatch matchA, matchB;
                      const bool aMatches = std::regex_match(pathStr, matchA, a->pattern);
                      const bool bMatches = std::regex_match(pathStr, matchB, b->pattern);

                      if (aMatches && bMatches) return matchA.str(0).size() > matchB.str(0).size();
                      return aMatches > bMatches;
                  });

        // Apply first matching override
        for (const auto& ovr : overrides)
            if (std::regex_match(pathStr, ovr->pattern)) return ovr->enabled;

        return isEnabled;
    }

    [[nodiscard]] bool canUploadFile(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canDownloadFile(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canDeleteFile(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canShareFilePublicly(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canShareFileInternally(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canLockFile(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canRenameFile(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canMoveFile(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canSyncFileLocally(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canSyncFileWithCloud(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canManageFileMetadata(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canChangeFileIcons(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canManageVersions(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canManageFileTags(const std::filesystem::path& path = {}) const;

    [[nodiscard]] bool canUploadDirectory(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canDownloadDirectory(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canDeleteDirectory(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canShareDirPublicly(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canShareDirInternally(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canLockDirectory(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canRenameDirectory(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canMoveDirectory(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canSyncDirectoryLocally(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canSyncDirectoryWithCloud(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canManageDirectoryMetadata(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canChangeDirectoryIcons(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canManageDirectoryTags(const std::filesystem::path& path = {}) const;
    [[nodiscard]] bool canListDirectory(const std::filesystem::path& path = {}) const;

};

// JSON + DB helpers
void to_json(nlohmann::json& j, const AssignedRole& r);

void from_json(const nlohmann::json& j, AssignedRole& r);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<AssignedRole> >& roles);

std::vector<std::shared_ptr<AssignedRole> > assigned_roles_from_json(const nlohmann::json& j);

std::vector<std::shared_ptr<AssignedRole> > assigned_roles_from_pq_result(const pqxx::result& res,
                                                                          const pqxx::result& overrides);

} // namespace vh::types