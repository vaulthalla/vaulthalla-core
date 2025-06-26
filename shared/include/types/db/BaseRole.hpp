#pragma once

#include <string>
#include <pqxx/row>
#include <cstdint>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace vh::types {

enum class PermissionName : uint16_t;

struct BaseRole {
    // roles
    unsigned int id;
    std::string name, display_name, description;
    uint16_t permissions;
    std::time_t created_at;

    BaseRole() = default;

    explicit BaseRole(const pqxx::row& row);
    explicit BaseRole(const nlohmann::json& j);

    [[nodiscard]] bool canManageUsers() const;
    [[nodiscard]] bool canManageRoles() const;
    [[nodiscard]] bool canManageStorage() const;
    [[nodiscard]] bool canManageFiles() const;
    [[nodiscard]] bool canViewAuditLog() const;
    [[nodiscard]] bool canUploadFile() const;
    [[nodiscard]] bool canDownloadFile() const;
    [[nodiscard]] bool canDeleteFile() const;
    [[nodiscard]] bool canShareFile() const;
    [[nodiscard]] bool canLockFile() const;
    [[nodiscard]] bool canManageSettings() const;
};

void to_json(nlohmann::json& j, const BaseRole& r);
void from_json(const nlohmann::json& j, BaseRole& r);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<BaseRole>>& roles);
std::vector<std::shared_ptr<BaseRole>> roles_from_json(const nlohmann::json& j);

} // namespace vh::types
