#include "types/User.hpp"
#include "types/AssignedRole.hpp"
#include "types/Permission.hpp"
#include "util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <pqxx/result>
#include <iostream>
#include <ranges>

namespace vh::types {

User::User() = default;

User::User(std::string name, std::string email)
    : name(std::move(name)), created_at(std::time(nullptr)) {
    if (email.empty()) this->email = std::nullopt;
    else this->email = std::move(email);
    password_hash = "";
    last_login = std::nullopt;
}

User::User(const pqxx::row& user, const pqxx::result& roles)
: id(user["id"].as<unsigned short>()),
  name(user["name"].as<std::string>()),
  password_hash(user["password_hash"].as<std::string>()),
  permissions(static_cast<uint16_t>(user["permissions"].as<int16_t>())),
  created_at(util::parsePostgresTimestamp(user["created_at"].as<std::string>())),
  last_login(user["last_login"].is_null()
                 ? std::nullopt
                 : std::make_optional(util::parsePostgresTimestamp(user["last_login"].as<std::string>()))),
  is_active(user["is_active"].as<bool>()),
  roles(assigned_roles_from_pq_result(roles)) {}

std::shared_ptr<AssignedRole> User::getRole(const unsigned int vaultId) const {
    const auto it = std::ranges::find_if(roles.begin(), roles.end(),
                       [vaultId](const std::shared_ptr<AssignedRole>& role) {
                           return role->scope_id.has_value() && role->scope_id.value() == vaultId;
                       });
    return it != roles.end() ? *it : nullptr;
}

void User::setPasswordHash(const std::string& hash) {
    password_hash = hash;
}

void User::updateUser(const nlohmann::json& j) {
    if (j.contains("name")) name = j.at("name").get<std::string>();
    if (j.contains("email")) email = j.at("email").get<std::string>();
    if (j.contains("is_active")) is_active = j.at("is_active").get<bool>();
    if (j.contains("permissions")) permissions = static_cast<uint16_t>(j[permissions].get<int16_t>());
}

bool User::isAdmin() const {
    return hasPermission(permissions, AdminPermission::CreateUser) ||
           hasPermission(permissions, AdminPermission::CreateAdminUser) ||
           hasPermission(permissions, AdminPermission::ManageRoles) ||
           hasPermission(permissions, AdminPermission::ManageSettings);
}

bool User::isSuperAdmin() const {
    return hasPermission(permissions, AdminPermission::CreateAdminUser);
}

// --- Admin checks ---
bool User::canCreateUser() const { return hasPermission(permissions, AdminPermission::CreateUser); }
bool User::canCreateAdminUser() const { return hasPermission(permissions, AdminPermission::CreateAdminUser); }
bool User::canDeactivateUser() const { return hasPermission(permissions, AdminPermission::DeactivateUser); }
bool User::canResetUserPassword() const { return hasPermission(permissions, AdminPermission::ResetUserPassword); }
bool User::canManageRoles() const { return hasPermission(permissions, AdminPermission::ManageRoles); }
bool User::canManageSettings() const { return hasPermission(permissions, AdminPermission::ManageSettings); }
bool User::canViewAuditLog() const { return hasPermission(permissions, AdminPermission::ViewAuditLog); }
bool User::canManageAPIKeys() const { return hasPermission(permissions, AdminPermission::ManageAPIKeys); }

// --- Vault checks ---
bool User::canCreateLocalVault() const { return hasPermission(vault_permissions, VaultPermission::CreateLocalVault); }
bool User::canCreateCloudVault() const { return hasPermission(vault_permissions, VaultPermission::CreateCloudVault); }
bool User::canDeleteVault() const { return hasPermission(vault_permissions, VaultPermission::DeleteVault); }
bool User::canAdjustVaultSettings() const { return hasPermission(vault_permissions, VaultPermission::AdjustVaultSettings); }
bool User::canMigrateVaultData() const { return hasPermission(vault_permissions, VaultPermission::MigrateVaultData); }
bool User::canCreateVolume() const { return hasPermission(vault_permissions, VaultPermission::CreateVolume); }
bool User::canDeleteVolume() const { return hasPermission(vault_permissions, VaultPermission::DeleteVolume); }
bool User::canResizeVolume() const { return hasPermission(vault_permissions, VaultPermission::ResizeVolume); }
bool User::canMoveVolume() const { return hasPermission(vault_permissions, VaultPermission::MoveVolume); }
bool User::canAssignVolumeToGroup() const { return hasPermission(vault_permissions, VaultPermission::AssignVolumeToGroup); }

// --- File checks ---
bool User::canUploadFile() const { return hasPermission(file_permissions, FilePermission::UploadFile); }
bool User::canDownloadFile() const { return hasPermission(file_permissions, FilePermission::DownloadFile); }
bool User::canDeleteFile() const { return hasPermission(file_permissions, FilePermission::DeleteFile); }
bool User::canShareFilePublicly() const { return hasPermission(file_permissions, FilePermission::ShareFilePublicly); }
bool User::canShareFileWithGroup() const { return hasPermission(file_permissions, FilePermission::ShareFileWithGroup); }
bool User::canLockFile() const { return hasPermission(file_permissions, FilePermission::LockFile); }
bool User::canRenameFile() const { return hasPermission(file_permissions, FilePermission::RenameFile); }
bool User::canMoveFile() const { return hasPermission(file_permissions, FilePermission::MoveFile); }

// --- Directory checks ---
bool User::canCreateDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::CreateDirectory); }
bool User::canDeleteDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::DeleteDirectory); }
bool User::canRenameDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::RenameDirectory); }
bool User::canMoveDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::MoveDirectory); }
bool User::canListDirectory() const { return hasPermission(directory_permissions, DirectoryPermission::ListDirectory); }

void to_json(nlohmann::json& j, const User& u) {
    j = {
        {"id", u.id},
        {"name", u.name},
        {"email", u.email},
        {"last_login", u.last_login.has_value() ? util::timestampToString(u.last_login.value()) : ""},
        {"created_at", util::timestampToString(u.created_at)},
        {"is_active", u.is_active}
    };

    if (u.global_role) j["global_role"] = *u.global_role;
    if (u.roles) j["scoped_roles"] = u.roles;
}

void from_json(const nlohmann::json& j, User& u) {
    u.id = j.at("id").get<unsigned short>();
    u.name = j.at("name").get<std::string>();
    u.email = j.at("email").get<std::string>();
    u.is_active = j.at("is_active").get<bool>();

    if (j.contains("global_role")) {
        if (j["global_role"].is_null()) u.global_role = nullptr;
        else u.global_role = std::make_shared<AssignedRole>(j["global_role"]);
    } else u.global_role = nullptr;

    // TODO: Handle scoped roles properly
    // if (j.contains("scoped_roles")) {
    //     if (j["scoped_roles"].is_null()) u.scoped_roles = std::nullopt;
    //     else u.scoped_roles = j["scoped_roles"];
    // } else {
    //     u.scoped_roles = std::nullopt;
    // }
}

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& user : users) j.push_back(*user);
    return j;
}

nlohmann::json to_json(const std::shared_ptr<User>& user) { return nlohmann::json(*user); }

} // namespace vh::types
