#include "types/User.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "types/Permission.hpp"
#include "types/Vault.hpp"
#include "util/timestamp.hpp"
#include "util/cmdLineHelpers.hpp"
#include "protocols/shell/Table.hpp"
#include "database/Queries/VaultQueries.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <pqxx/result>
#include <ranges>

using namespace vh::shell;
using namespace vh::database;
using namespace vh::util;

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
      linux_uid(row["linux_uid"].as<std::optional<unsigned int>>()),
      name(row["name"].as<std::string>()),
      password_hash(row["password_hash"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())),
      is_active(row["is_active"].as<bool>()),
      role(nullptr) {
    if (row["last_login"].is_null()) last_login = std::nullopt;
    else last_login = std::make_optional(util::parsePostgresTimestamp(row["last_login"].as<std::string>()));
    if (row["email"].is_null()) email = std::nullopt;
    else email = std::make_optional(row["email"].as<std::string>());
    if (row["last_modified_by"].is_null()) last_modified_by = std::nullopt;
    else last_modified_by = std::make_optional(row["last_modified_by"].as<unsigned int>());
}

User::User(const pqxx::row& user, const pqxx::row& role, const pqxx::result& vaultRoles, const pqxx::result& overrides)
: User(user) {
    this->role = std::make_shared<UserRole>(role);
    const auto [roles, group_roles] = vault_roles_from_pq_result(vaultRoles, overrides);
    this->roles = roles;
    this->group_roles = group_roles;
}

bool User::operator==(const User& other) const {
    return linux_uid == other.linux_uid &&
           name == other.name &&
           email == other.email;
}

bool User::operator!=(const User& other) const {
    return !(*this == other);
}

std::shared_ptr<VaultRole> User::getRole(const unsigned int vaultId) const {
    if (roles.contains(vaultId)) return roles.at(vaultId);
    if (group_roles.contains(vaultId)) return group_roles.at(vaultId);
    return nullptr;
}

void User::setPasswordHash(const std::string& hash) {
    password_hash = hash;
}

void User::updateUser(const nlohmann::json& j) {
    if (j.contains("name")) name = j.at("name").get<std::string>();
    if (j.contains("email")) email = j.at("email").get<std::string>();
    if (j.contains("is_active")) is_active = j.at("is_active").get<bool>();
}

void to_json(nlohmann::json& j, const User& u) {
    j = {
        {"id", u.id},
        {"name", u.name},
        {"email", u.email},
        {"last_login", u.last_login.has_value() ? timestampToString(u.last_login.value()) : ""},
        {"created_at", timestampToString(u.created_at)},
        {"is_active", u.is_active},
        {"role", *u.role}
    };

    if (u.linux_uid) j["uid"] = *u.linux_uid;

    if (!u.roles.empty()) j["roles"] = u.roles;
}

void from_json(const nlohmann::json& j, User& u) {
    u.id = j.at("id").get<unsigned short>();
    u.linux_uid = j.at("uid").get<std::optional<unsigned int>>();
    u.name = j.at("name").get<std::string>();
    u.email = j.at("email").get<std::string>();
    u.is_active = j.at("is_active").get<bool>();
    if (j.contains("role")) u.role = std::make_shared<UserRole>(j.at("role"));
}

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& user : users) j.push_back(*user);
    return j;
}

nlohmann::json to_json(const std::shared_ptr<User>& user) { return {*user}; }


// --- User role checks ---
bool User::isSuperAdmin() const {
    return hasPermission(role->permissions, AdminPermission::ManageAdmins)
    && hasPermission(role->permissions, AdminPermission::ManageEncryptionKeys);
}
bool User::isAdmin() const { return isSuperAdmin() || role->permissions == ADMIN_MASK; }


// --- Admin checks ---
bool User::canManageEncryptionKeys() const { return hasPermission(role->permissions, AdminPermission::ManageEncryptionKeys); }
bool User::canManageAdmins() const { return hasPermission(role->permissions, AdminPermission::ManageAdmins); }
bool User::canManageUsers() const { return hasPermission(role->permissions, AdminPermission::ManageUsers); }
bool User::canManageGroups() const { return hasPermission(role->permissions, AdminPermission::ManageGroups); }
bool User::canManageRoles() const { return hasPermission(role->permissions, AdminPermission::ManageRoles); }
bool User::canManageSettings() const { return hasPermission(role->permissions, AdminPermission::ManageSettings); }
bool User::canManageVaults() const { return hasPermission(role->permissions, AdminPermission::ManageVaults); }
bool User::canAccessAuditLog() const { return hasPermission(role->permissions, AdminPermission::AuditLogAccess); }
bool User::canManageAPIKeys() const { return hasPermission(role->permissions, AdminPermission::ManageAPIKeys); }
bool User::canCreateVaults() const { return hasPermission(role->permissions, AdminPermission::CreateVaults); }


// --- Vault role->permissions ---

bool User::canManageVault(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canManageVault(path);
}

bool User::canManageVaultAccess(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canManageAccess(path);
}

bool User::canManageVaultTags(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canManageTags(path);
}

bool User::canManageVaultMetadata(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canManageMetadata(path);
}

bool User::canManageVaultVersions(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canManageVersions(path);
}

bool User::canManageVaultFileLocks(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canManageFileLocks(path);
}

bool User::canShareVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canShare(path);
}

bool User::canSyncVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canSync(path);
}

bool User::canCreateVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canCreate(path);
}

bool User::canDownloadVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->canDownload(path);
}

bool User::canDeleteVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->validatePermission(role->permissions, VaultPermission::Delete, path);
}

bool User::canRenameVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->validatePermission(role->permissions, VaultPermission::Rename, path);
}

bool User::canMoveVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->validatePermission(role->permissions, VaultPermission::Move, path);
}

bool User::canListVaultData(const unsigned int vaultId, const std::filesystem::path& path) const {
    if (VaultQueries::getVaultOwnerId(vaultId) == id) return true;
    const auto role = getRole(vaultId);
    return role && role->validatePermission(role->permissions, VaultPermission::List, path);
}

std::string to_string(const std::shared_ptr<User>& user) {
    if (!user) return "No user found\n";

    std::string out;
    out += "User ID: " + std::to_string(user->id) + "\n";
    out += "User: " + user->name + "\n";
    out += "Email: " + user->email.value_or("N/A") + "\n";
    if (user->linux_uid) out += "Linux UID: " + std::to_string(*user->linux_uid) + "\n";
    else out += "Linux UID: Not set\n";
    out += "Created At: " + util::timestampToString(user->created_at) + "\n";
    out += "Last Login: " + (user->last_login ? util::timestampToString(*user->last_login) : "Never") + "\n";
    out += "Active: " + std::string(user->is_active ? "Yes" : "No") + "\n";
    out += "Role: " + to_string(user->role);
    out += "Vault Roles:\n";
    out += "  - User Level:\n";
    out += "    " + to_string(user->roles);
    out += "  - Group Level:\n";
    out += "    " + to_string(user->group_roles);

    return out;
}

std::string to_string(const std::vector<std::shared_ptr<User>>& users) {
    if (users.empty()) return "No users found\n";

    Table tbl({
        {"ID", Align::Right, 4, 4, false, false},
        {"Name", Align::Left, 4, 32, false, false},
        {"Email", Align::Left, 4, 32, false, false},
        {"Role", Align::Left, 4, 16, false, false},
        {"Created At", Align::Left, 4, 20, false, false},
        {"Last Login", Align::Left, 4, 20, false, false},
        {"Active", Align::Left, 4, 8, false, false}
    }, term_width());

    for (const auto& user : users) {
        if (!user) continue; // Skip null pointers
        tbl.add_row({
            std::to_string(user->id),
            user->name,
            user->email.value_or("N/A"),
            user->role ? snake_case_to_title(user->role->name) : "No Role",
            util::timestampToString(user->created_at),
            user->last_login ? util::timestampToString(*user->last_login) : "Never",
            user->is_active ? "Yes" : "No"
        });
    }

    return "Vaulthalla users:\n" + tbl.render();
}

} // namespace vh::types
