#pragma once

#include "rbac/role/Meta.hpp"
#include "rbac/permission/Vault.hpp"

#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <regex>
#include <optional>
#include <unordered_map>

namespace vh::rbac::role::vault {

struct Override;

struct Global final : BasicMeta {
    enum class Scope { Self, User, Admin };

    uint32_t user_id{};
    Scope scope{Scope::Self};
    std::optional<uint32_t> template_role_id;
    bool enforce_template{false};
    permission::Vault permissions{};

    Global() = default;

    explicit Global(const pqxx::row& row);
    explicit Global(const nlohmann::json& j);

    [[nodiscard]] std::string toString(uint8_t indent) const override;
    [[nodiscard]] std::string toString() const { return toString(0); }

    [[nodiscard]] permission::vault::fs::Files& files() noexcept { return permissions.filesystem.files; }
    [[nodiscard]] const permission::vault::fs::Files& files() const noexcept { return permissions.filesystem.files; }

    [[nodiscard]] permission::vault::fs::Directories& directories() noexcept { return permissions.filesystem.directories; }
    [[nodiscard]] const permission::vault::fs::Directories& directories() const noexcept { return permissions.filesystem.directories; }

    [[nodiscard]] permission::vault::Roles& rolesPerms() noexcept { return permissions.roles; }
    [[nodiscard]] const permission::vault::Roles& rolesPerms() const noexcept { return permissions.roles; }

    [[nodiscard]] permission::vault::sync::Config& syncConfig() noexcept { return permissions.sync.config; }
    [[nodiscard]] const permission::vault::sync::Config& syncConfig() const noexcept { return permissions.sync.config; }

    [[nodiscard]] permission::vault::sync::Action& syncActions() noexcept { return permissions.sync.action; }
    [[nodiscard]] const permission::vault::sync::Action& syncActions() const noexcept { return permissions.sync.action; }

    static Global fromJson(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const Global& r);
void from_json(const nlohmann::json& j, Global& r);

std::vector<Global> global_vault_roles_from_json(const nlohmann::json& j);
std::vector<Global> global_vault_roles_from_pq_result(const pqxx::result& res);

std::string to_string(const Global& role);

std::string to_string(const Global::Scope& scope);
Global::Scope global_vault_role_scope_from_string(const std::string& name);

}
