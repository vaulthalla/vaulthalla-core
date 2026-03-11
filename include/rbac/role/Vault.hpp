#pragma once

#include "Base.hpp"
#include "rbac/permission/Vault.hpp"
#include "rbac/permission/Override.hpp"
#include "log/Registry.hpp"
#include "fs/model/Path.hpp"

#include <string>
#include <ctime>
#include <filesystem>
#include <memory>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <regex>
#include <unordered_map>

namespace vh::rbac::role {

struct Override;

struct Vault final : Base {
    uint32_t subject_id{}, role_id{}, vault_id{};
    std::string subject_type; // 'user' or 'group'
    permission::Vault permissions{};

    Vault() = default;
    ~Vault() override = default;

    Vault(const pqxx::row& row, const pqxx::result& overrides);

    Vault(const pqxx::row& row, const std::vector<pqxx::row>& overrides);

    explicit Vault(const nlohmann::json& j);
    explicit Vault(const Base& r) : Base(r) {
        if (type != "vault") throw std::runtime_error("VaultRole: invalid role type");
    }

    static std::shared_ptr<Vault> fromJson(const nlohmann::json& j);

    [[nodiscard]] std::string permissions_to_flags_string() const override;

    [[nodiscard]] std::vector<std::shared_ptr<Override> > getPermissionOverrides(unsigned short bit) const;

    template <typename T> bool validatePermission(const uint16_t mask, T perm,
                                                  const std::filesystem::path& path = {}) const {
        const bool isEnabled = (mask & static_cast<uint16_t>(perm)) != 0;
        if (path.empty()) return isEnabled;

        const auto pathStr = fs::model::makeAbsolute(path).string();
        const auto bit = vaultPermToBit(perm);
        auto overrides = getPermissionOverrides(bit);
        if (overrides.empty()) {
            log::Registry::auth()->debug("[VaultRole::validatePermission] No overrides for permission on path {}", pathStr);
            return isEnabled;
        }

        // Sort: best match first
        std::sort(overrides.begin(), overrides.end(),
                  [&pathStr](const std::shared_ptr<Override>& a, const std::shared_ptr<Override>& b) {
                      std::smatch matchA, matchB;
                      const bool aMatches = std::regex_match(pathStr, matchA, a->pattern);
                      const bool bMatches = std::regex_match(pathStr, matchB, b->pattern);

                      if (aMatches && bMatches) return matchA.str(0).size() > matchB.str(0).size();
                      return aMatches > bMatches;
                  });

        // Apply first matching override
        for (const auto& ovr : overrides)
            if (std::regex_match(pathStr, ovr->pattern)) {
                if (!ovr->enabled) continue;
                return ovr->effect == permission::OverrideOpt::ALLOW;
            }

        return isEnabled;
    }

    [[nodiscard]] bool canManageVault(const std::filesystem::path& path) const;
    [[nodiscard]] bool canManageAccess(const std::filesystem::path& path) const;
    [[nodiscard]] bool canManageTags(const std::filesystem::path& path) const;
    [[nodiscard]] bool canManageMetadata(const std::filesystem::path& path) const;
    [[nodiscard]] bool canManageVersions(const std::filesystem::path& path) const;
    [[nodiscard]] bool canManageFileLocks(const std::filesystem::path& path) const;
    [[nodiscard]] bool canShare(const std::filesystem::path& path) const;
    [[nodiscard]] bool canSync(const std::filesystem::path& path) const;
    [[nodiscard]] bool canCreate(const std::filesystem::path& path) const;
    [[nodiscard]] bool canDownload(const std::filesystem::path& path) const;
    [[nodiscard]] bool canDelete(const std::filesystem::path& path) const;
    [[nodiscard]] bool canRename(const std::filesystem::path& path) const;
    [[nodiscard]] bool canMove(const std::filesystem::path& path) const;
    [[nodiscard]] bool canList(const std::filesystem::path& path) const;

};

struct VRolePair {
    std::unordered_map<unsigned int, std::shared_ptr<Vault>> roles, group_roles;
};

// JSON + DB helpers
void to_json(nlohmann::json& j, const Vault& r);

void from_json(const nlohmann::json& j, Vault& r);

void to_json(nlohmann::json& j, const std::unordered_map<unsigned int, std::shared_ptr<Vault>>& roles);

VRolePair vault_roles_from_json(const nlohmann::json& j);
VRolePair vault_roles_from_pq_result(const pqxx::result& res, const pqxx::result& overrides);

std::string to_string(const std::shared_ptr<Vault>& role);
std::string to_string(const std::unordered_map<unsigned int, std::shared_ptr<Vault>>& roles);
std::string to_string(const std::vector<std::shared_ptr<Vault>>& roles);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Vault> >& roles);

std::vector<std::shared_ptr<Vault>> vault_roles_vector_from_json(const nlohmann::json& j);

std::vector<std::shared_ptr<Vault>> vault_roles_vector_from_pq_result(const pqxx::result& res,
                                                                          const pqxx::result& overrides);

}