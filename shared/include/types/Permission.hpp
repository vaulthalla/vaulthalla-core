#pragma once

#include <string>
#include <cstdint>
#include <ctime>
#include <vector>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <pqxx/row>

namespace vh::types {

enum class AdminPermission : uint16_t {
    ManageAdmins            = 1ULL << 0,
    ManageUsers             = 1ULL << 1,
    ManageRoles             = 1ULL << 2,
    ManageSettings          = 1ULL << 3,
    ManageVaults            = 1ULL << 4,   // create, delete, adjust settings of any vault
    AuditLogAccess          = 1ULL << 5,
    FullAPIKeyAccess        = 1ULL << 6,   // manage API keys, create, delete, list - users may only see their own keys
};

enum class VaultPermission : uint16_t {
    MigrateData             = 1ULL << 0,   // migrate vault data to another storage backend
    ManageAccess            = 1ULL << 1,   // manage vault roles, assign users/groups to vault roles
    ManageTags              = 1ULL << 2,   // manage tags for files and directories in the vault
    ManageMetadata          = 1ULL << 3,
    ManageVersions          = 1ULL << 4,
    ManageFileLocks         = 1ULL << 5,
    Share                   = 1ULL << 6,   // public links; internal share is managed by Vault ACL
    Sync                    = 1ULL << 7,   // sync vault with cloud storage, internal is managed by Vault ACL
    Create                  = 1ULL << 8,   // create files/directories and upload files
    Download                = 1ULL << 9,   // download files, read file contents
    Delete                  = 1ULL << 10,
    Rename                  = 1ULL << 11,
    Move                    = 1ULL << 12,
    List                    = 1ULL << 13,  // must be set at top-level, use overrides to restrict specific directories
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
inline unsigned short vaultPermToBit(const VaultPermission& perm) { return static_cast<unsigned short>(perm); }

// String conversions
std::string to_string(AdminPermission p);
std::string to_string(VaultPermission p);

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
nlohmann::json jsonFromVaultMask(uint16_t mask);

uint16_t adminMaskFromJson(const nlohmann::json& j);
uint16_t vaultMaskFromJson(const nlohmann::json& j);

} // namespace vh::types
