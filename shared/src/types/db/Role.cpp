#include "types/db/Role.hpp"
#include "util/timestamp.hpp"
#include "types/db/Permission.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

Role::Role(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      role_id(row["role_id"].as<unsigned int>()),
      subject_id(row["subject_id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      display_name(row["display_name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      scope(row["scope"].as<std::string>()),
      scope_id(row["scope_id"].is_null() ? std::nullopt : std::make_optional(row["scope_id"].as<unsigned int>())),
      admin_permissions(static_cast<uint16_t>(row["admin_permissions"].as<int64_t>())),
      vault_permissions(static_cast<uint16_t>(row["vault_permissions"].as<int64_t>())),
      file_permissions(static_cast<uint16_t>(row["file_permissions"].as<int64_t>())),
      directory_permissions(static_cast<uint16_t>(row["directory_permissions"].as<int64_t>())),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      assigned_at(util::parsePostgresTimestamp(row["assigned_at"].as<std::string>())),
      inherited(row["inherited"].as<bool>()) {}

Role::Role(const nlohmann::json& j)
    : id(j.at("id").get<unsigned int>()),
      role_id(j.at("role_id").get<unsigned int>()),
      subject_id(j.at("subject_id").get<unsigned int>()),
      name(j.at("name").get<std::string>()),
      display_name(j.at("display_name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      scope(j.at("scope").get<std::string>()),
      scope_id(j.contains("scope_id") && !j["scope_id"].is_null() ? std::make_optional(j["scope_id"].get<unsigned int>()) : std::nullopt),
      admin_permissions(j.at("admin_permissions").get<uint16_t>()),
      vault_permissions(j.at("vault_permissions").get<uint16_t>()),
      file_permissions(j.at("file_permissions").get<uint16_t>()),
      directory_permissions(j.at("directory_permissions").get<uint16_t>()),
      created_at(util::parsePostgresTimestamp(j.at("created_at").get<std::string>())),
      assigned_at(util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>())),
      inherited(j.value("inherited", false)) {}

void vh::types::to_json(nlohmann::json& j, const Role& r) {
    j = {
        {"id", r.id},
        {"role_id", r.role_id},
        {"subject_id", r.subject_id},
        {"name", r.name},
        {"display_name", r.display_name},
        {"description", r.description},
        {"scope", r.scope},
        {"admin_permissions", stringArrayFromAdminMask(r.admin_permissions)},
        {"vault_permissions", stringArrayFromVaultMask(r.vault_permissions)},
        {"file_permissions", stringArrayFromFileMask(r.file_permissions)},
        {"directory_permissions", stringArrayFromDirectoryMask(r.directory_permissions)},
        {"created_at", util::timestampToString(r.created_at)},
        {"assigned_at", util::timestampToString(r.assigned_at)},
        {"inherited", r.inherited}
    };
    if (r.scope_id.has_value()) {
        j["scope_id"] = r.scope_id.value();
    }
}

void vh::types::from_json(const nlohmann::json& j, Role& r) {
    r.id = j.at("id").get<unsigned int>();
    r.role_id = j.at("role_id").get<unsigned int>();
    r.subject_id = j.at("subject_id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    r.display_name = j.at("display_name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.scope = j.at("scope").get<std::string>();
    r.scope_id = j.contains("scope_id") && !j["scope_id"].is_null()
                   ? std::make_optional(j.at("scope_id").get<unsigned int>())
                   : std::nullopt;
    r.admin_permissions = j.at("admin_permissions").get<uint16_t>();
    r.vault_permissions = j.at("vault_permissions").get<uint16_t>();
    r.file_permissions = j.at("file_permissions").get<uint16_t>();
    r.directory_permissions = j.at("directory_permissions").get<uint16_t>();
    r.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
    r.assigned_at = util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>());
    r.inherited = j.value("inherited", false);
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) {
        j.push_back(*role);
    }
}

std::vector<std::shared_ptr<Role>> vh::types::roles_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<Role>> roles;
    for (const auto& roleJson : j) {
        roles.push_back(std::make_shared<Role>(roleJson));
    }
    return roles;
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
bool Role::canAdjustVaultSettings() const { return hasPermission(vault_permissions, VaultPermission::AdjustVaultSettings); }
bool Role::canMigrateVaultData() const { return hasPermission(vault_permissions, VaultPermission::MigrateVaultData); }
bool Role::canCreateVolume() const { return hasPermission(vault_permissions, VaultPermission::CreateVolume); }
bool Role::canDeleteVolume() const { return hasPermission(vault_permissions, VaultPermission::DeleteVolume); }
bool Role::canResizeVolume() const { return hasPermission(vault_permissions, VaultPermission::ResizeVolume); }
bool Role::canMoveVolume() const { return hasPermission(vault_permissions, VaultPermission::MoveVolume); }
bool Role::canAssignVolumeToGroup() const { return hasPermission(vault_permissions, VaultPermission::AssignVolumeToGroup); }

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
bool Role::canCreateDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::CreateDirectory); }
bool Role::canDeleteDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::DeleteDirectory); }
bool Role::canRenameDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::RenameDirectory); }
bool Role::canMoveDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::MoveDirectory); }
bool Role::canListDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::ListDirectory); }
