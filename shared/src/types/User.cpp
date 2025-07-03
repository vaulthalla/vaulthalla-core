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

User::User(std::string name, std::string email, const bool isActive)
    : name(std::move(name)), created_at(std::time(nullptr)), is_active(isActive) {
    if (email.empty()) this->email = std::nullopt;
    else this->email = std::move(email);
    password_hash = "";
    last_login = std::nullopt;
}

User::User(const pqxx::row& row)
    : id(row["id"].as<unsigned short>()),
      name(row["name"].as<std::string>()),
      password_hash(row["password_hash"].as<std::string>()),
      permissions(static_cast<uint16_t>(row["permissions"].as<int16_t>())),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      is_active(row["is_active"].as<bool>()) {
    if (row["last_login"].is_null()) last_login = std::nullopt;
    else last_login = std::make_optional(util::parsePostgresTimestamp(row["last_login"].as<std::string>()));
    if (row["email"].is_null()) email = std::nullopt;
    else email = std::make_optional(row["email"].as<std::string>());
}

User::User(const pqxx::row& user, const pqxx::result& roles)
: User(user) { this->roles = assigned_roles_from_pq_result(roles); }

std::shared_ptr<AssignedRole> User::getRole(const unsigned int vaultId) const {
    const auto it = std::ranges::find_if(roles.begin(), roles.end(),
                       [vaultId](const std::shared_ptr<AssignedRole>& role) {
                           return role->vault_id == vaultId;
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

void to_json(nlohmann::json& j, const User& u) {
    j = {
        {"id", u.id},
        {"name", u.name},
        {"email", u.email},
        {"last_login", u.last_login.has_value() ? util::timestampToString(u.last_login.value()) : ""},
        {"created_at", util::timestampToString(u.created_at)},
        {"is_active", u.is_active}
    };

    if (!u.roles.empty()) j["roles"] = u.roles;
}

void from_json(const nlohmann::json& j, User& u) {
    u.id = j.at("id").get<unsigned short>();
    u.name = j.at("name").get<std::string>();
    u.email = j.at("email").get<std::string>();
    u.is_active = j.at("is_active").get<bool>();
}

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& user : users) j.push_back(*user);
    return j;
}

nlohmann::json to_json(const std::shared_ptr<User>& user) { return nlohmann::json(*user); }


// --- User role checks ---

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
bool User::canCreateLocalVault() const { return hasPermission(permissions, AdminPermission::CreateLocalVault); }
bool User::canCreateCloudVault() const { return hasPermission(permissions, AdminPermission::CreateCloudVault); }
bool User::canDeleteVault() const { return hasPermission(permissions, AdminPermission::DeleteVault); }
bool User::canManageVaultSettings() const { return hasPermission(permissions, AdminPermission::ManageVaultSettings); }
bool User::canManageVaultRoles() const { return hasPermission(permissions, AdminPermission::ManageVaultRoles); }
bool User::canManageAllVaults() const { return hasPermission(permissions, AdminPermission::ManageAllVaults); }
bool User::canMigrateVaultData() const { return hasPermission(permissions, AdminPermission::MigrateVaultData); }

// --- Vault permissions ---
bool User::canUploadFile(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canUploadFile();
}

bool User::canDownloadFile(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canDownloadFile();
}

bool User::canDeleteFile(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canDeleteFile();
}

bool User::canShareFilePublicly(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canShareFilePublicly();
}

bool User::canShareFileInternally(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canShareFileInternally();
}

bool User::canLockFile(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canLockFile();
}

bool User::canRenameFile(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canRenameFile();
}

bool User::canMoveFile(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canMoveFile();
}

bool User::canSyncFileLocally(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canSyncFileLocally();
}

bool User::canSyncFileWithCloud(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canSyncFileWithCloud();
}

bool User::canManageFileMetadata(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canManageFileMetadata();
}

bool User::canChangeFileIcons(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canChangeFileIcons();
}

bool User::canManageVersions(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canManageVersions();
}

bool User::canManageFileTags(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canManageFileTags();
}

bool User::canUploadDirectory(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canUploadDirectory();
}

bool User::canDownloadDirectory(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canDownloadDirectory();
}

bool User::canDeleteDirectory(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canDeleteDirectory();
}

bool User::canShareDirPublicly(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canShareDirPublicly();
}

bool User::canShareDirInternally(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canShareDirInternally();
}

bool User::canLockDirectory(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canLockDirectory();
}

bool User::canRenameDirectory(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canRenameDirectory();
}

bool User::canMoveDirectory(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canMoveDirectory();
}

bool User::canSyncDirectoryLocally(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canSyncDirectoryLocally();
}

bool User::canSyncDirectoryWithCloud(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canSyncDirectoryWithCloud();
}

bool User::canManageDirectoryMetadata(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canManageDirectoryMetadata();
}

bool User::canChangeDirectoryIcons(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canChangeDirectoryIcons();
}

bool User::canManageDirectoryTags(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canManageDirectoryTags();
}

bool User::canListDirectory(const unsigned int vaultId) const {
    const auto role = getRole(vaultId);
    return role && role->canListDirectory();
}

} // namespace vh::types
