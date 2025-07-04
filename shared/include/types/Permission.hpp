#pragma once

#include <string>
#include <cstdint>
#include <ctime>
#include <vector>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <pqxx/row>

namespace vh::types {

// Admin-level permissions
enum class AdminPermission : uint16_t {
    CreateUser              = 1ULL << 0,
    CreateAdminUser         = 1ULL << 1,
    DeactivateUser          = 1ULL << 2,
    ResetUserPassword       = 1ULL << 3,
    ManageRoles             = 1ULL << 4,
    ManageSettings          = 1ULL << 5,
    ViewAuditLog            = 1ULL << 6,
    ManageAPIKeys           = 1ULL << 7,
    CreateLocalVault        = 1ULL << 8,
    CreateCloudVault        = 1ULL << 9,
    DeleteVault             = 1ULL << 10,
    ManageVaultSettings     = 1ULL << 11,
    ManageVaultRoles        = 1ULL << 12,
    MigrateVaultData        = 1ULL << 13,
    ManageAllVaults         = 1ULL << 14,
};

// File and Directory permissions
enum class FSPermission : uint16_t {
    Upload                  = 1ULL << 0,
    Download                = 1ULL << 1,
    Delete                  = 1ULL << 2,
    SharePublic             = 1ULL << 3,
    ShareInternal           = 1ULL << 4,
    Lock                    = 1ULL << 5,
    Rename                  = 1ULL << 6,
    Move                    = 1ULL << 7,
    SyncLocal               = 1ULL << 8,
    SyncCloud               = 1ULL << 9,
    ModifyMetadata          = 1ULL << 10,
    ChangeIcons             = 1ULL << 11,
    ManageTags              = 1ULL << 12,
    List                    = 1ULL << 13,  // Directory specific
    ManageVersions          = 1ULL << 14,  // File specific
};

struct Permission {
    unsigned int id;
    std::string name, display_name, description;
    uint16_t bit_position;
    std::time_t created_at, updated_at;

    explicit Permission(const pqxx::row& row);
    explicit Permission(const nlohmann::json& j);
};

inline unsigned short adminPermToBit(const AdminPermission& perm) { return static_cast<unsigned short>(perm); }
inline unsigned short fsPermToBit(const FSPermission& perm) { return static_cast<unsigned short>(perm); }

// String conversions
std::string to_string(AdminPermission p);
std::string to_string(FSPermission p);

// JSON serialization
void to_json(nlohmann::json& j, const Permission& p);
void from_json(const nlohmann::json& j, Permission& p);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Permission>>& permissions);

inline std::string bitStringFromMask(const uint16_t mask) {
    std::string out = "B'";
    for (int i = 15; i >= 0; --i) out += (mask & (1 << i)) ? '1' : '0';
    out += "'";
    return out;
}

// Bitmask utilities
template <typename T>
uint16_t toBitmask(const std::vector<T>& perms) {
    uint16_t mask = 0;
    for (auto p : perms) mask |= static_cast<uint16_t>(p);
    return mask;
}

template <typename T>
std::vector<T> permsFromBitmask(uint16_t mask) {
    std::vector<T> result;
    for (uint16_t bit = 0; bit < 64; ++bit) {
        uint16_t val = (1ULL << bit);
        if ((mask & val) != 0) {
            result.push_back(static_cast<T>(val));
        }
    }
    return result;
}

template <typename T>
bool hasPermission(const uint16_t mask, T perm) { return (mask & static_cast<uint16_t>(perm)) != 0; }

nlohmann::json jsonFromAdminMask(uint16_t mask);
nlohmann::json jsonFromFSMask(uint16_t mask);

uint16_t adminMaskFromJson(const nlohmann::json& j);
uint16_t fsMaskFromJson(const nlohmann::json& j);

} // namespace vh::types
