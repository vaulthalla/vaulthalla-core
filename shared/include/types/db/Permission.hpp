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
    CreateUser           = 1ULL << 0,
    CreateAdminUser      = 1ULL << 1,
    DeactivateUser       = 1ULL << 2,
    ResetUserPassword    = 1ULL << 3,
    ManageRoles          = 1ULL << 4,
    ManageSettings       = 1ULL << 5,
    ViewAuditLog         = 1ULL << 6,
    ManageAPIKeys        = 1ULL << 7
};

// Vault + volume permissions
enum class VaultPermission : uint16_t {
    CreateLocalVault     = 1ULL << 0,
    CreateCloudVault     = 1ULL << 1,
    DeleteVault          = 1ULL << 2,
    AdjustVaultSettings  = 1ULL << 3,
    MigrateVaultData     = 1ULL << 4,
    CreateVolume         = 1ULL << 5,
    DeleteVolume         = 1ULL << 6,
    ResizeVolume         = 1ULL << 7,
    MoveVolume           = 1ULL << 8,
    AssignVolumeToGroup  = 1ULL << 9
};

// File-level permissions
enum class FilePermission : uint16_t {
    UploadFile           = 1ULL << 0,
    DownloadFile         = 1ULL << 1,
    DeleteFile           = 1ULL << 2,
    ShareFilePublicly    = 1ULL << 3,
    ShareFileWithGroup   = 1ULL << 4,
    LockFile             = 1ULL << 5,
    RenameFile           = 1ULL << 6,
    MoveFile             = 1ULL << 7
};

// Directory-level permissions
enum class DirectoryPermission : uint16_t {
    CreateDirectory      = 1ULL << 0,
    DeleteDirectory      = 1ULL << 1,
    RenameDirectory      = 1ULL << 2,
    MoveDirectory        = 1ULL << 3,
    ListDirectory        = 1ULL << 4
};

struct Permission {
    unsigned int id;
    std::string name;
    std::string display_name;
    std::string description;
    uint16_t bit_position;
    std::time_t created_at;
    std::time_t updated_at;

    explicit Permission(const pqxx::row& row);
};

// String conversions
std::string to_string(AdminPermission p);
std::string to_string(VaultPermission p);
std::string to_string(FilePermission p);
std::string to_string(DirectoryPermission p);

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
    for (auto p : perms) {
        mask |= static_cast<uint16_t>(p);
    }
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
bool hasPermission(uint16_t mask, T perm) {
    return (mask & static_cast<uint16_t>(perm)) != 0;
}

inline std::vector<std::string> stringArrayFromAdminMask(const uint16_t mask) {
    std::vector<std::string> out;
    if (mask & static_cast<uint16_t>(AdminPermission::CreateUser)) out.push_back("Create User");
    if (mask & static_cast<uint16_t>(AdminPermission::CreateAdminUser)) out.push_back("Create Admin User");
    if (mask & static_cast<uint16_t>(AdminPermission::DeactivateUser)) out.push_back("Deactivate User");
    if (mask & static_cast<uint16_t>(AdminPermission::ResetUserPassword)) out.push_back("Reset User Password");
    if (mask & static_cast<uint16_t>(AdminPermission::ManageRoles)) out.push_back("Manage Roles");
    if (mask & static_cast<uint16_t>(AdminPermission::ManageSettings)) out.push_back("Manage Settings");
    if (mask & static_cast<uint16_t>(AdminPermission::ViewAuditLog)) out.push_back("View Audit Log");
    if (mask & static_cast<uint16_t>(AdminPermission::ManageAPIKeys)) out.push_back("Manage API Keys");
    return out;
}

inline std::vector<std::string> stringArrayFromVaultMask(const uint16_t mask) {
    std::vector<std::string> out;
    if (mask & static_cast<uint16_t>(VaultPermission::CreateLocalVault)) out.push_back("Create Local Vault");
    if (mask & static_cast<uint16_t>(VaultPermission::CreateCloudVault)) out.push_back("Create Cloud Vault");
    if (mask & static_cast<uint16_t>(VaultPermission::DeleteVault)) out.push_back("Delete Vault");
    if (mask & static_cast<uint16_t>(VaultPermission::AdjustVaultSettings)) out.push_back("Adjust Vault Settings");
    if (mask & static_cast<uint16_t>(VaultPermission::MigrateVaultData)) out.push_back("Migrate Vault Data");
    if (mask & static_cast<uint16_t>(VaultPermission::CreateVolume)) out.push_back("Create Volume");
    if (mask & static_cast<uint16_t>(VaultPermission::DeleteVolume)) out.push_back("Delete Volume");
    if (mask & static_cast<uint16_t>(VaultPermission::ResizeVolume)) out.push_back("Resize Volume");
    if (mask & static_cast<uint16_t>(VaultPermission::MoveVolume)) out.push_back("Move Volume");
    if (mask & static_cast<uint16_t>(VaultPermission::AssignVolumeToGroup)) out.push_back("Assign Volume to Group");
    return out;
}

inline std::vector<std::string> stringArrayFromFileMask(const uint16_t mask) {
    std::vector<std::string> out;
    if (mask & static_cast<uint16_t>(FilePermission::UploadFile)) out.push_back("Upload File");
    if (mask & static_cast<uint16_t>(FilePermission::DownloadFile)) out.push_back("Download File");
    if (mask & static_cast<uint16_t>(FilePermission::DeleteFile)) out.push_back("Delete File");
    if (mask & static_cast<uint16_t>(FilePermission::ShareFilePublicly)) out.push_back("Share File Publicly");
    if (mask & static_cast<uint16_t>(FilePermission::ShareFileWithGroup)) out.push_back("Share File With Group");
    if (mask & static_cast<uint16_t>(FilePermission::LockFile)) out.push_back("Lock File");
    if (mask & static_cast<uint16_t>(FilePermission::RenameFile)) out.push_back("Rename File");
    if (mask & static_cast<uint16_t>(FilePermission::MoveFile)) out.push_back("Move File");
    return out;
}

inline std::vector<std::string> stringArrayFromDirectoryMask(const uint16_t mask) {
    std::vector<std::string> out;
    if (mask & static_cast<uint16_t>(DirectoryPermission::CreateDirectory)) out.push_back("Create Directory");
    if (mask & static_cast<uint16_t>(DirectoryPermission::DeleteDirectory)) out.push_back("Delete Directory");
    if (mask & static_cast<uint16_t>(DirectoryPermission::RenameDirectory)) out.push_back("Rename Directory");
    if (mask & static_cast<uint16_t>(DirectoryPermission::MoveDirectory)) out.push_back("Move Directory");
    if (mask & static_cast<uint16_t>(DirectoryPermission::ListDirectory)) out.push_back("List Directory");
    return out;
}

nlohmann::json jsonFromAdminMask(uint16_t mask);
nlohmann::json jsonFromVaultMask(uint16_t mask);
nlohmann::json jsonFromFileMask(uint16_t mask);
nlohmann::json jsonFromDirectoryMask(uint16_t mask);

uint16_t adminMaskFromJson(const nlohmann::json& j);
uint16_t vaultMaskFromJson(const nlohmann::json& j);
uint16_t fileMaskFromJson(const nlohmann::json& j);
uint16_t directoryMaskFromJson(const nlohmann::json& j);

} // namespace vh::types
