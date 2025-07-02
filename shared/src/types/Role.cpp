#include "types/Role.hpp"
#include "util/timestamp.hpp"
#include "types/Permission.hpp"

#include <pqxx/row>
#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types;

Role::Role(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      file_permissions(static_cast<uint16_t>(row["file_permissions"].as<int64_t>())),
      directory_permissions(static_cast<uint16_t>(row["directory_permissions"].as<int64_t>())),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {
}

Role::Role(const nlohmann::json& j)
    : id(j.contains("id") ? j.at("id").get<unsigned int>() : 0),
      name(j.at("name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      file_permissions(fileMaskFromJson(j.at("file_permissions"))),
      directory_permissions(directoryMaskFromJson(j.at("directory_permissions"))),
      created_at(static_cast<std::time_t>(0)){
}

void vh::types::to_json(nlohmann::json& j, const Role& r) {
    j = {
        {"id", r.id},
        {"name", r.name},
        {"description", r.description},
        {"file_permissions", jsonFromFileMask(r.file_permissions)},
        {"directory_permissions", jsonFromDirectoryMask(r.directory_permissions)},
        {"created_at", util::timestampToString(r.created_at)}
    };
}

void vh::types::from_json(const nlohmann::json& j, Role& r) {
    if (j.contains("id")) r.id = j.at("id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.file_permissions = fileMaskFromJson(j.at("file_permissions"));
    r.directory_permissions = directoryMaskFromJson(j.at("directory_permissions"));
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role> >& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

std::vector<std::shared_ptr<Role>> vh::types::roles_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<Role>> roles;
    for (const auto& item : res) roles.push_back(std::make_shared<Role>(item));
    return roles;
}

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