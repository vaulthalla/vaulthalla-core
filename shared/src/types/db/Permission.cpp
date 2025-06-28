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

nlohmann::json jsonFromAdminMask(const uint16_t mask) {
    return {
        { "create_user",            (mask & (1 << 0)) != 0 },
        { "create_admin_user",      (mask & (1 << 1)) != 0 },
        { "deactivate_user",        (mask & (1 << 2)) != 0 },
        { "reset_user_password",    (mask & (1 << 3)) != 0 },
        { "manage_roles",           (mask & (1 << 4)) != 0 },
        { "manage_settings",        (mask & (1 << 5)) != 0 },
        { "view_audit_log",         (mask & (1 << 6)) != 0 },
        { "manage_api_keys",        (mask & (1 << 7)) != 0 }
    };
}

nlohmann::json jsonFromVaultMask(const uint16_t mask) {
    return {
        { "create_local_vault",     (mask & (1 << 0)) != 0 },
        { "create_cloud_vault",     (mask & (1 << 1)) != 0 },
        { "delete_vault",           (mask & (1 << 2)) != 0 },
        { "adjust_vault_settings",  (mask & (1 << 3)) != 0 },
        { "migrate_vault_data",     (mask & (1 << 4)) != 0 },
        { "create_volume",          (mask & (1 << 5)) != 0 },
        { "delete_volume",          (mask & (1 << 6)) != 0 },
        { "resize_volume",          (mask & (1 << 7)) != 0 },
        { "move_volume",            (mask & (1 << 8)) != 0 },
        { "assign_volume_to_group", (mask & (1 << 9)) != 0 }
    };
}

nlohmann::json jsonFromFileMask(const uint16_t mask) {
    return {
        { "upload_file",            (mask & (1 << 0)) != 0 },
        { "download_file",          (mask & (1 << 1)) != 0 },
        { "delete_file",            (mask & (1 << 2)) != 0 },
        { "share_file_publicly",    (mask & (1 << 3)) != 0 },
        { "share_file_with_group",  (mask & (1 << 4)) != 0 },
        { "lock_file",              (mask & (1 << 5)) != 0 },
        { "rename_file",            (mask & (1 << 6)) != 0 },
        { "move_file",              (mask & (1 << 7)) != 0 }
    };
}

nlohmann::json jsonFromDirectoryMask(const uint16_t mask) {
    return {
        { "create_directory",       (mask & (1 << 0)) != 0 },
        { "delete_directory",       (mask & (1 << 1)) != 0 },
        { "rename_directory",       (mask & (1 << 2)) != 0 },
        { "move_directory",         (mask & (1 << 3)) != 0 },
        { "list_directory",         (mask & (1 << 4)) != 0 }
    };
}

uint16_t adminMaskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("create_user").get<bool>()) mask |= (1 << 0);
    if (j.at("create_admin_user").get<bool>()) mask |= (1 << 1);
    if (j.at("deactivate_user").get<bool>()) mask |= (1 << 2);
    if (j.at("reset_user_password").get<bool>()) mask |= (1 << 3);
    if (j.at("manage_roles").get<bool>()) mask |= (1 << 4);
    if (j.at("manage_settings").get<bool>()) mask |= (1 << 5);
    if (j.at("view_audit_log").get<bool>()) mask |= (1 << 6);
    if (j.at("manage_api_keys").get<bool>()) mask |= (1 << 7);
    return mask;
}

uint16_t vaultMaskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("create_local_vault").get<bool>()) mask |= (1 << 0);
    if (j.at("create_cloud_vault").get<bool>()) mask |= (1 << 1);
    if (j.at("delete_vault").get<bool>()) mask |= (1 << 2);
    if (j.at("adjust_vault_settings").get<bool>()) mask |= (1 << 3);
    if (j.at("migrate_vault_data").get<bool>()) mask |= (1 << 4);
    if (j.at("create_volume").get<bool>()) mask |= (1 << 5);
    if (j.at("delete_volume").get<bool>()) mask |= (1 << 6);
    if (j.at("resize_volume").get<bool>()) mask |= (1 << 7);
    if (j.at("move_volume").get<bool>()) mask |= (1 << 8);
    if (j.at("assign_volume_to_group").get<bool>()) mask |= (1 << 9);
    return mask;
}

uint16_t fileMaskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("upload_file").get<bool>()) mask |= (1 << 0);
    if (j.at("download_file").get<bool>()) mask |= (1 << 1);
    if (j.at("delete_file").get<bool>()) mask |= (1 << 2);
    if (j.at("share_file_publicly").get<bool>()) mask |= (1 << 3);
    if (j.at("share_file_with_group").get<bool>()) mask |= (1 << 4);
    if (j.at("lock_file").get<bool>()) mask |= (1 << 5);
    if (j.at("rename_file").get<bool>()) mask |= (1 << 6);
    if (j.at("move_file").get<bool>()) mask |= (1 << 7);
    return mask;
}

uint16_t directoryMaskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("create_directory").get<bool>()) mask |= (1 << 0);
    if (j.at("delete_directory").get<bool>()) mask |= (1 << 1);
    if (j.at("rename_directory").get<bool>()) mask |= (1 << 2);
    if (j.at("move_directory").get<bool>()) mask |= (1 << 3);
    if (j.at("list_directory").get<bool>()) mask |= (1 << 4);
    return mask;
}

}
