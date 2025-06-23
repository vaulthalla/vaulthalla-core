#include "types/db/User.hpp"
#include "types/db/Role.hpp"

#include "util/timestamp.hpp"
#include <nlohmann/json.hpp>
#include <pqxx/row>

namespace vh::types {

User::User() = default;

User::User(std::string name, std::string email, bool isActive, const RoleName& role)
    : id(0), name(std::move(name)), email(std::move(email)), is_active(isActive), role(role) {
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
      role(role_from_db_string(row["role"].as<std::string>())) {}

void User::setPasswordHash(const std::string& hash) {
    password_hash = hash;
}

bool User::isAdmin() const {
    return role == RoleName::Admin || role == RoleName::SuperAdmin;
}

bool User::isUser() const {
    return role == RoleName::User || isAdmin();
}

bool User::isGuest() const {
    return role == RoleName::Guest;
}

bool User::isModerator() const {
    return role == RoleName::Moderator || isAdmin();
}

void to_json(nlohmann::json& j, const User& u) {
    j = {
        {"id", u.id},
        {"name", u.name},
        {"email", u.email},
        {"last_login", u.last_login.has_value() ? util::timestampToString(u.last_login.value()) : ""},
        {"created_at", util::timestampToString(u.created_at)},
        {"is_active", u.is_active},
        {"role", to_cli_string(u.role)},
    };
}

void from_json(const nlohmann::json& j, User& u) {
    j.at("name").get_to(u.name);
    j.at("email").get_to(u.email);
}

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& user : users) j.push_back(*user);
    return j;
}

nlohmann::json to_json(const std::shared_ptr<User>& user) { return nlohmann::json(*user); }

} // namespace vh::types
