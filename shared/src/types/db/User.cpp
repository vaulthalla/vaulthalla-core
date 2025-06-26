#include "types/db/User.hpp"
#include <iostream>

#include "types/db/Role.hpp"

#include "util/timestamp.hpp"
#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <pqxx/result>

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
        scoped_roles = std::vector<std::shared_ptr<Role>>();
        for (const auto& role_row : roles) {
            const auto role = std::make_shared<Role>(role_row);
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
        else global_role = std::make_shared<Role>(j["global_role"]);
    }

    if (j.contains("scoped_roles")) {
        if (j["scoped_roles"].is_null()) scoped_roles = std::nullopt;
        else scoped_roles = user_roles_from_json(j["scoped_roles"]);
    }
}

bool User::canManageUsers() const { return global_role && global_role->canManageUsers(); }
bool User::canManageRoles() const { return global_role && global_role->canManageRoles(); }
bool User::canManageStorage() const { return global_role && global_role->canManageStorage(); }
bool User::canManageFiles() const { return global_role && global_role->canManageFiles(); }
bool User::canViewAuditLog() const { return global_role && global_role->canViewAuditLog(); }
bool User::canUploadFile() const { return global_role && global_role->canUploadFile(); }
bool User::canDownloadFile() const { return global_role && global_role->canDownloadFile(); }
bool User::canDeleteFile() const { return global_role && global_role->canDeleteFile(); }
bool User::canShareFile() const { return global_role && global_role->canShareFile(); }
bool User::canLockFile() const { return global_role && global_role->canLockFile(); }
bool User::canManageSettings() const { return global_role && global_role->canManageSettings(); }

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
        else u.global_role = std::make_shared<Role>(j["global_role"]);
    } else u.global_role = nullptr;

    if (j.contains("scoped_roles")) {
        if (j["scoped_roles"].is_null()) u.scoped_roles = std::nullopt;
        else u.scoped_roles = user_roles_from_json(j["scoped_roles"]);
    } else {
        u.scoped_roles = std::nullopt;
    }
}

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& user : users) j.push_back(*user);
    return j;
}

nlohmann::json to_json(const std::shared_ptr<User>& user) { return nlohmann::json(*user); }

} // namespace vh::types
