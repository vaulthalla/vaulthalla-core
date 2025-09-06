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
    ManageEncryptionKeys    = 1ULL << 0, // export vault encryption keys for backup or migration
    ManageAdmins            = 1ULL << 1, // manage admin users, create, delete, update admin accounts
    ManageUsers             = 1ULL << 2, // manage user accounts, create, delete, update user accounts
    ManageGroups            = 1ULL << 3, // manage groups, create, delete, update groups
    ManageRoles             = 1ULL << 4, // manage roles, create, delete, update roles
    ManageSettings          = 1ULL << 5, // manage system settings, configuration, and policies
    ManageVaults            = 1ULL << 6, // manage vaults, create, delete, update vault settings
    ManageAPIKeys           = 1ULL << 7,
    AuditLogAccess          = 1ULL << 8, // access audit logs for monitoring
    CreateVaults            = 1ULL << 9,
};

enum class VaultPermission : uint16_t {
    ManageVault             = 1ULL << 0,   // manage vault settings, including sync & upstream encryption settings
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
    unsigned int id{};
    std::string name, description;
    uint16_t bit_position{};
    std::time_t created_at{}, updated_at{};

    Permission() = default;
    explicit Permission(const pqxx::row& row);
    explicit Permission(const nlohmann::json& j);
    Permission(unsigned int bitPos, std::string name, std::string description);
};

inline unsigned short adminPermToBit(const AdminPermission& perm) { return static_cast<unsigned short>(perm); }
inline unsigned short vaultPermToBit(const VaultPermission& perm) { return static_cast<unsigned short>(perm); }

// String conversions
std::string to_string(AdminPermission p);
std::string to_string(VaultPermission p);
std::string get_vault_perm_name(const VaultPermission& p);

// JSON serialization
void to_json(nlohmann::json& j, const Permission& p);
void from_json(const nlohmann::json& j, Permission& p);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Permission>>& permissions);

inline std::string bitStringFromMask(const uint16_t mask) {
    std::string out = "B";
    for (int i = 15; i >= 0; --i) out += (mask & (1 << i)) ? '1' : '0';
    out += "";
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

std::string admin_perms_to_string(uint16_t mask, unsigned short indent = 4);
std::string vault_perms_to_string(uint16_t mask, unsigned short indent = 4);

} // namespace vh::types
