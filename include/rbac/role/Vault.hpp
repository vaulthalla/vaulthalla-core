#pragma once

#include "rbac/role/Meta.hpp"
#include "vault/Base.hpp"
#include "rbac/permission/Permission.hpp"

#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <regex>
#include <memory>
#include <optional>
#include <unordered_map>

namespace vh::rbac::role {

struct Vault final : Meta, vault::Base {
    struct AssignmentInfo {
        uint32_t subject_id{}, vault_id{};
        std::string subject_type; // 'user' or 'group'

        [[nodiscard]] std::string toString(uint8_t indent) const;
    };

    std::optional<AssignmentInfo> assignment;

    Vault() = default;
    Vault(const pqxx::row& row, const pqxx::result& overrides);
    explicit Vault(const nlohmann::json& j);

    [[nodiscard]] std::vector<permission::Permission> toPermissions() const;

    [[nodiscard]] std::string toString(uint8_t indent) const override;
    [[nodiscard]] std::string toString() const { return toString(0); }
    [[nodiscard]] std::string toFlagsString() const;

    [[nodiscard]] permission::vault::fs::Files& files() noexcept { return fs.files; }
    [[nodiscard]] const permission::vault::fs::Files& files() const noexcept { return fs.files; }

    [[nodiscard]] permission::vault::fs::Directories& directories() noexcept { return fs.directories; }
    [[nodiscard]] const permission::vault::fs::Directories& directories() const noexcept { return fs.directories; }

    [[nodiscard]] permission::vault::Roles& rolesPerms() noexcept { return roles; }
    [[nodiscard]] const permission::vault::Roles& rolesPerms() const noexcept { return roles; }

    [[nodiscard]] permission::vault::sync::Config& syncConfig() noexcept { return sync.config; }
    [[nodiscard]] const permission::vault::sync::Config& syncConfig() const noexcept { return sync.config; }

    [[nodiscard]] permission::vault::sync::Action& syncActions() noexcept { return sync.action; }
    [[nodiscard]] const permission::vault::sync::Action& syncActions() const noexcept { return sync.action; }

    static Vault fromJson(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const Vault& r);
void from_json(const nlohmann::json& j, Vault& r);

void to_json(nlohmann::json& j, const std::vector<Vault>& roles);
void to_json(nlohmann::json& j, const std::unordered_map<uint32_t, std::shared_ptr<Vault>>& roles);

void to_json(nlohmann::json& j, const Vault::AssignmentInfo& r);
void from_json(const nlohmann::json& j, Vault::AssignmentInfo& r);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Vault>>& roles);

std::vector<Vault> vault_roles_from_json(const nlohmann::json& j);
std::unordered_map<uint32_t, std::shared_ptr<Vault>> vault_roles_from_pq_result(const pqxx::result& res, const pqxx::result& overrides);

std::string to_string(const Vault& role);
std::string to_string(const std::vector<Vault>& roles);

}