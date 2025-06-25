#pragma once

#include <string>
#include <pqxx/row>
#include <cstdint>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace vh::types {

enum class PermissionName : uint16_t;

struct Role {
    // roles
    unsigned int id;
    std::string name, display_name, description;
    uint16_t permissions;
    std::time_t created_at;

    Role() = default;

    explicit Role(const pqxx::row& row);
    explicit Role(const nlohmann::json& j);

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
};

void to_json(nlohmann::json& j, const Role& r);
void from_json(const nlohmann::json& j, Role& r);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& roles);
std::vector<std::shared_ptr<Role>> roles_from_json(const nlohmann::json& j);

} // namespace vh::types
