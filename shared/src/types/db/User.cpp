#include "types/db/User.hpp"
#include "types/db/AssignedRole.hpp"
#include "types/db/Permission.hpp"

#include "util/timestamp.hpp"
#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <pqxx/result>
#include <iostream>

namespace vh::types {

User::User() = default;

User::User(std::string name, std::string email, const bool isActive)
    : id(0), name(std::move(name)), email(std::move(email)), is_active(isActive), global_role(nullptr) {
    created_at = std::time(nullptr);
    last_login = std::nullopt;
}

User::User(const pqxx::row& row)
    : id(row["id"].as<unsigned short>()),
      name(row["name"].as<std::string>()),
      email(row["email"].as<std::string>()),
      password_hash(row["password_hash"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      last_login(row["last_login"].is_null()
                     ? std::nullopt
                     : std::make_optional(util::parsePostgresTimestamp(row["last_login"].as<std::string>()))),
      is_active(row["is_active"].as<bool>()),
      global_role(nullptr),
      scoped_roles(std::nullopt) {}

User::User(const pqxx::row& user, const pqxx::result& roles)
    : id(user["id"].as<unsigned short>()),
      name(user["name"].as<std::string>()),
      email(user["email"].as<std::string>()),
      password_hash(user["password_hash"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(user["created_at"].as<std::string>())),
      last_login(user["last_login"].is_null()
                     ? std::nullopt
                     : std::make_optional(util::parsePostgresTimestamp(user["last_login"].as<std::string>()))),
      is_active(user["is_active"].as<bool>()),
      global_role(nullptr),
      scoped_roles(std::nullopt) {
    if (!roles.empty()) {
        scoped_roles = std::vector<std::shared_ptr<AssignedRole>>();
        for (const auto& role_row : roles) {
            const auto role = std::make_shared<AssignedRole>(role_row);
            if (role->scope == "global") global_role = role;
            else scoped_roles->push_back(role);
        }
    }

    if (!global_role) std::cerr << "User does not have a global role." << std::endl;
}

void User::setPasswordHash(const std::string& hash) {
    password_hash = hash;
}

void User::updateUser(const nlohmann::json& j) {
    if (j.contains("name")) name = j.at("name").get<std::string>();
    if (j.contains("email")) email = j.at("email").get<std::string>();
    if (j.contains("is_active")) is_active = j.at("is_active").get<bool>();

    if (j.contains("global_role")) {
        if (j["global_role"].is_null()) global_role = nullptr;
        else global_role = std::make_shared<AssignedRole>(j["global_role"]);
    }

    // TODO: Handle scoped roles properly
    // if (j.contains("scoped_roles")) {
    //     if (j["scoped_roles"].is_null()) scoped_roles = std::nullopt;
    //     else scoped_roles = user_roles_from_json(j["scoped_roles"]);
    // }
}

// --- Admin checks ---
bool User::canCreateUser() const { return hasPermission(global_role->admin_permissions, AdminPermission::CreateUser); }
bool User::canCreateAdminUser() const { return hasPermission(global_role->admin_permissions, AdminPermission::CreateAdminUser); }
bool User::canDeactivateUser() const { return hasPermission(global_role->admin_permissions, AdminPermission::DeactivateUser); }
bool User::canResetUserPassword() const { return hasPermission(global_role->admin_permissions, AdminPermission::ResetUserPassword); }
bool User::canManageRoles() const { return hasPermission(global_role->admin_permissions, AdminPermission::ManageRoles); }
bool User::canManageSettings() const { return hasPermission(global_role->admin_permissions, AdminPermission::ManageSettings); }
bool User::canViewAuditLog() const { return hasPermission(global_role->admin_permissions, AdminPermission::ViewAuditLog); }
bool User::canManageAPIKeys() const { return hasPermission(global_role->admin_permissions, AdminPermission::ManageAPIKeys); }

// --- Vault checks ---
bool User::canCreateLocalVault() const { return hasPermission(global_role->vault_permissions, VaultPermission::CreateLocalVault); }
bool User::canCreateCloudVault() const { return hasPermission(global_role->vault_permissions, VaultPermission::CreateCloudVault); }
bool User::canDeleteVault() const { return hasPermission(global_role->vault_permissions, VaultPermission::DeleteVault); }
bool User::canAdjustVaultSettings() const { return hasPermission(global_role->vault_permissions, VaultPermission::AdjustVaultSettings); }
bool User::canMigrateVaultData() const { return hasPermission(global_role->vault_permissions, VaultPermission::MigrateVaultData); }
bool User::canCreateVolume() const { return hasPermission(global_role->vault_permissions, VaultPermission::CreateVolume); }
bool User::canDeleteVolume() const { return hasPermission(global_role->vault_permissions, VaultPermission::DeleteVolume); }
bool User::canResizeVolume() const { return hasPermission(global_role->vault_permissions, VaultPermission::ResizeVolume); }
bool User::canMoveVolume() const { return hasPermission(global_role->vault_permissions, VaultPermission::MoveVolume); }
bool User::canAssignVolumeToGroup() const { return hasPermission(global_role->vault_permissions, VaultPermission::AssignVolumeToGroup); }

// --- File checks ---
bool User::canUploadFile() const { return hasPermission(global_role->file_permissions, FilePermission::UploadFile); }
bool User::canDownloadFile() const { return hasPermission(global_role->file_permissions, FilePermission::DownloadFile); }
bool User::canDeleteFile() const { return hasPermission(global_role->file_permissions, FilePermission::DeleteFile); }
bool User::canShareFilePublicly() const { return hasPermission(global_role->file_permissions, FilePermission::ShareFilePublicly); }
bool User::canShareFileWithGroup() const { return hasPermission(global_role->file_permissions, FilePermission::ShareFileWithGroup); }
bool User::canLockFile() const { return hasPermission(global_role->file_permissions, FilePermission::LockFile); }
bool User::canRenameFile() const { return hasPermission(global_role->file_permissions, FilePermission::RenameFile); }
bool User::canMoveFile() const { return hasPermission(global_role->file_permissions, FilePermission::MoveFile); }

// --- Directory checks ---
bool User::canCreateDirectory() const { return hasPermission(global_role->directory_permissions, DirectoryPermission::CreateDirectory); }
bool User::canDeleteDirectory() const { return hasPermission(global_role->directory_permissions, DirectoryPermission::DeleteDirectory); }
bool User::canRenameDirectory() const { return hasPermission(global_role->directory_permissions, DirectoryPermission::RenameDirectory); }
bool User::canMoveDirectory() const { return hasPermission(global_role->directory_permissions, DirectoryPermission::MoveDirectory); }
bool User::canListDirectory() const { return hasPermission(global_role->directory_permissions, DirectoryPermission::ListDirectory); }

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
    if (u.scoped_roles) j["scoped_roles"] = u.scoped_roles;
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
