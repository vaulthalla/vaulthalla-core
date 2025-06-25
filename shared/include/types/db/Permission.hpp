#pragma once

#include <string>
#include <cstdint>
#include <ctime>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <pqxx/row>

namespace vh::types {

enum class PermissionName : uint16_t {
    ManageUsers = 1 << 0,
    ManageRoles = 1 << 1,
    ManageStorage = 1 << 2,
    ManageFiles = 1 << 3,
    ViewAuditLog = 1 << 4,
    UploadFile = 1 << 5,
    DownloadFile = 1 << 6,
    DeleteFile = 1 << 7,
    ShareFile = 1 << 8,
    LockFile = 1 << 9,
    ManageSettings = 1 << 10,
};

struct Permission {
    static constexpr unsigned short BITMAP_SIZE = 11;

    unsigned int id;
    PermissionName name;
    std::string display_name, description;
    unsigned short bit_position;
    std::time_t created_at;
    std::time_t updated_at;

    explicit Permission(const pqxx::row& row);
};

std::string to_string(const PermissionName& permission);
nlohmann::json to_json(const std::vector<PermissionName>& permissions);

void to_json(nlohmann::json& j, const Permission& p);
void from_json(const nlohmann::json& j, Permission& p);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Permission>>& permissions);

uint16_t toBitmask(const std::vector<PermissionName>& permissions);
std::vector<PermissionName> permsFromBitmask(uint16_t bitmask);
std::vector<std::string> permsFromBitmaskAsString(uint16_t bitmask);
bool hasPermission(uint16_t bitmask, PermissionName permission);

} // namespace vh::types
