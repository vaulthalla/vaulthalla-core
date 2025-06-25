#include "types/db/Permission.hpp"
#include "util/timestamp.hpp"

#include <string>
#include <nlohmann/json.hpp>

using namespace vh::types;

Permission::Permission(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(static_cast<PermissionName>(row["name"].as<uint16_t>())),
      description(row["description"].as<std::string>()),
      bit_position(row["bit_position"].as<unsigned short>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())) {}

std::string vh::types::to_string(const PermissionName& permission) {
    switch (permission) {
        case PermissionName::ManageUsers: return "Manage Users";
        case PermissionName::ManageRoles: return "Manage Roles";
        case PermissionName::ManageStorage: return "Manage Storage";
        case PermissionName::ManageFiles: return "Manage Files";
        case PermissionName::ViewAuditLog: return "View Audit Log";
        case PermissionName::UploadFile: return "Upload File";
        case PermissionName::DownloadFile: return "Download File";
        case PermissionName::DeleteFile: return "Delete File";
        case PermissionName::ShareFile: return "Share File";
        case PermissionName::LockFile: return "Lock File";
        default: return "Unknown Permission";
    }
}

nlohmann::json vh::types::to_json(const std::vector<PermissionName>& permissions) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& perm : permissions) {
        j.push_back({
            {"name", to_string(perm)},
            {"bit_position", static_cast<unsigned short>(perm)},
        });
    }
    return j;
}

uint16_t vh::types::toBitmask(const std::vector<PermissionName>& permissions) {
    uint16_t bitmask = 0;
    for (const auto& perm : permissions) bitmask |= static_cast<uint16_t>(perm);
    return bitmask;
}

std::vector<PermissionName> vh::types::permsFromBitmask(const uint16_t bitmask) {
    std::vector<PermissionName> permissions;
    for (uint16_t i = 0; i < 16; ++i) if (bitmask & (1 << i)) permissions.push_back(static_cast<PermissionName>(1 << i));
    return permissions;
}

bool vh::types::hasPermission(const uint16_t bitmask, PermissionName permission) {
    return (bitmask & static_cast<uint16_t>(permission)) != 0;
}
