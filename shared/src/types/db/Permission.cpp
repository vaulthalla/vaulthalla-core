#include "types/db/Permission.hpp"
#include "util/timestamp.hpp"
#include <nlohmann/json.hpp>

namespace vh::types {

Permission::Permission(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      display_name(row["display_name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      bit_position(row["bit_position"].as<uint16_t>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())) {}

std::string to_string(const AdminPermission p) {
    switch (p) {
        case AdminPermission::CreateUser: return "Create User";
        case AdminPermission::CreateAdminUser: return "Create Admin User";
        case AdminPermission::DeactivateUser: return "Deactivate User";
        case AdminPermission::ResetUserPassword: return "Reset User Password";
        case AdminPermission::ManageRoles: return "Manage Roles";
        case AdminPermission::ManageSettings: return "Manage Settings";
        case AdminPermission::ViewAuditLog: return "View Audit Log";
        case AdminPermission::ManageAPIKeys: return "Manage API Keys";
        default: return "Unknown Admin Permission";
    }
}

std::string to_string(const VaultPermission p) {
    switch (p) {
        case VaultPermission::CreateLocalVault: return "Create Local Vault";
        case VaultPermission::CreateCloudVault: return "Create Cloud Vault";
        case VaultPermission::DeleteVault: return "Delete Vault";
        case VaultPermission::AdjustVaultSettings: return "Adjust Vault Settings";
        case VaultPermission::MigrateVaultData: return "Migrate Vault Data";
        case VaultPermission::CreateVolume: return "Create Volume";
        case VaultPermission::DeleteVolume: return "Delete Volume";
        case VaultPermission::ResizeVolume: return "Resize Volume";
        case VaultPermission::MoveVolume: return "Move Volume";
        case VaultPermission::AssignVolumeToGroup: return "Assign Volume to Group";
        default: return "Unknown Vault Permission";
    }
}

std::string to_string(const FilePermission p) {
    switch (p) {
        case FilePermission::UploadFile: return "Upload File";
        case FilePermission::DownloadFile: return "Download File";
        case FilePermission::DeleteFile: return "Delete File";
        case FilePermission::ShareFilePublicly: return "Share File Publicly";
        case FilePermission::ShareFileWithGroup: return "Share File With Group";
        case FilePermission::LockFile: return "Lock File";
        case FilePermission::RenameFile: return "Rename File";
        case FilePermission::MoveFile: return "Move File";
        default: return "Unknown File Permission";
    }
}

std::string to_string(const DirectoryPermission p) {
    switch (p) {
        case DirectoryPermission::CreateDirectory: return "Create Directory";
        case DirectoryPermission::DeleteDirectory: return "Delete Directory";
        case DirectoryPermission::RenameDirectory: return "Rename Directory";
        case DirectoryPermission::MoveDirectory: return "Move Directory";
        case DirectoryPermission::ListDirectory: return "List Directory";
        default: return "Unknown Directory Permission";
    }
}

void to_json(nlohmann::json& j, const Permission& p) {
    j = {
        {"id", p.id},
        {"name", p.name},
        {"display_name", p.display_name},
        {"description", p.description},
        {"bit_position", p.bit_position},
        {"created_at", vh::util::timestampToString(p.created_at)},
        {"updated_at", vh::util::timestampToString(p.updated_at)}
    };
}

void from_json(const nlohmann::json& j, Permission& p) {
    p.id = j.at("id").get<unsigned int>();
    p.name = j.at("name").get<std::string>();
    p.display_name = j.at("display_name").get<std::string>();
    p.description = j.at("description").get<std::string>();
    p.bit_position = j.at("bit_position").get<uint16_t>();
}

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Permission>>& permissions) {
    j = nlohmann::json::array();
    for (const auto& perm : permissions) j.push_back(*perm);
}

}
