#include "types/db/Role.hpp"
#include "util/timestamp.hpp"
#include "types/db/Permission.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

Role::Role(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      display_name(row["display_name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      admin_permissions(static_cast<uint16_t>(row["admin_permissions"].as<int64_t>())),
      vault_permissions(static_cast<uint16_t>(row["vault_permissions"].as<int64_t>())),
      file_permissions(static_cast<uint16_t>(row["file_permissions"].as<int64_t>())),
      directory_permissions(static_cast<uint16_t>(row["directory_permissions"].as<int64_t>())),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {
}

Role::Role(const nlohmann::json& j)
    : id(j.contains("id") ? j.at("id").get<unsigned int>() : 0),
      name(j.at("name").get<std::string>()),
      display_name(j.contains("display_name") ? j.at("display_name").get<std::string>() : ""),
      description(j.at("description").get<std::string>()),
      admin_permissions(adminMaskFromJson(j.at("admin_permissions"))),
      vault_permissions(vaultMaskFromJson(j.at("vault_permissions"))),
      file_permissions(fileMaskFromJson(j.at("file_permissions"))),
      directory_permissions(directoryMaskFromJson(j.at("directory_permissions"))),
      created_at(static_cast<std::time_t>(0)){
}

void vh::types::to_json(nlohmann::json& j, const Role& r) {
    j = {
        {"id", r.id},
        {"name", r.name},
        {"display_name", r.display_name},
        {"description", r.description},
        {"admin_permissions", jsonFromAdminMask(r.admin_permissions)},
        {"vault_permissions", jsonFromVaultMask(r.vault_permissions)},
        {"file_permissions", jsonFromFileMask(r.file_permissions)},
        {"directory_permissions", jsonFromDirectoryMask(r.directory_permissions)},
        {"created_at", util::timestampToString(r.created_at)}
    };
}

void vh::types::from_json(const nlohmann::json& j, Role& r) {
    if (j.contains("id")) r.id = j.at("id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    if (j.contains("display_name")) r.display_name = j.at("display_name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.admin_permissions = adminMaskFromJson(j.at("admin_permissions"));
    r.vault_permissions = vaultMaskFromJson(j.at("vault_permissions"));
    r.file_permissions = fileMaskFromJson(j.at("file_permissions"));
    r.directory_permissions = directoryMaskFromJson(j.at("directory_permissions"));
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role> >& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

bool Role::isAdmin() const {
    return hasPermission(admin_permissions, AdminPermission::CreateUser) ||
           hasPermission(admin_permissions, AdminPermission::CreateAdminUser) ||
           hasPermission(admin_permissions, AdminPermission::ManageRoles) ||
           hasPermission(admin_permissions, AdminPermission::ManageSettings);
}

bool Role::isSuperAdmin() const {
    return hasPermission(admin_permissions, AdminPermission::CreateAdminUser);
}

// --- Admin checks ---
bool Role::canCreateUser() const { return hasPermission(admin_permissions, AdminPermission::CreateUser); }
bool Role::canCreateAdminUser() const { return hasPermission(admin_permissions, AdminPermission::CreateAdminUser); }
bool Role::canDeactivateUser() const { return hasPermission(admin_permissions, AdminPermission::DeactivateUser); }
bool Role::canResetUserPassword() const { return hasPermission(admin_permissions, AdminPermission::ResetUserPassword); }
bool Role::canManageRoles() const { return hasPermission(admin_permissions, AdminPermission::ManageRoles); }
bool Role::canManageSettings() const { return hasPermission(admin_permissions, AdminPermission::ManageSettings); }
bool Role::canViewAuditLog() const { return hasPermission(admin_permissions, AdminPermission::ViewAuditLog); }
bool Role::canManageAPIKeys() const { return hasPermission(admin_permissions, AdminPermission::ManageAPIKeys); }

// --- Vault checks ---
bool Role::canCreateLocalVault() const { return hasPermission(vault_permissions, VaultPermission::CreateLocalVault); }
bool Role::canCreateCloudVault() const { return hasPermission(vault_permissions, VaultPermission::CreateCloudVault); }
bool Role::canDeleteVault() const { return hasPermission(vault_permissions, VaultPermission::DeleteVault); }

bool Role::canAdjustVaultSettings() const {
    return hasPermission(vault_permissions, VaultPermission::AdjustVaultSettings);
}

bool Role::canMigrateVaultData() const { return hasPermission(vault_permissions, VaultPermission::MigrateVaultData); }
bool Role::canCreateVolume() const { return hasPermission(vault_permissions, VaultPermission::CreateVolume); }
bool Role::canDeleteVolume() const { return hasPermission(vault_permissions, VaultPermission::DeleteVolume); }
bool Role::canResizeVolume() const { return hasPermission(vault_permissions, VaultPermission::ResizeVolume); }
bool Role::canMoveVolume() const { return hasPermission(vault_permissions, VaultPermission::MoveVolume); }

bool Role::canAssignVolumeToGroup() const {
    return hasPermission(vault_permissions, VaultPermission::AssignVolumeToGroup);
}

// --- File checks ---
bool Role::canUploadFile() const { return hasPermission(file_permissions, FilePermission::UploadFile); }
bool Role::canDownloadFile() const { return hasPermission(file_permissions, FilePermission::DownloadFile); }
bool Role::canDeleteFile() const { return hasPermission(file_permissions, FilePermission::DeleteFile); }
bool Role::canShareFilePublicly() const { return hasPermission(file_permissions, FilePermission::ShareFilePublicly); }
bool Role::canShareFileWithGroup() const { return hasPermission(file_permissions, FilePermission::ShareFileWithGroup); }
bool Role::canLockFile() const { return hasPermission(file_permissions, FilePermission::LockFile); }
bool Role::canRenameFile() const { return hasPermission(file_permissions, FilePermission::RenameFile); }
bool Role::canMoveFile() const { return hasPermission(file_permissions, FilePermission::MoveFile); }

// --- Directory checks ---
bool Role::canCreateDirectory() const {
    return hasPermission(directory_permissions, DirectoryPermission::CreateDirectory);
}

bool Role::canDeleteDirectory() const {
    return hasPermission(directory_permissions, DirectoryPermission::DeleteDirectory);
}

bool Role::canRenameDirectory() const {
    return hasPermission(directory_permissions, DirectoryPermission::RenameDirectory);
}

bool Role::canMoveDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::MoveDirectory); }
bool Role::canListDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::ListDirectory); }