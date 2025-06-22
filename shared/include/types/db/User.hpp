#pragma once

#include "util/timestamp.hpp"
#include <boost/describe.hpp>
#include <ctime>
#include <nlohmann/json.hpp>
#include <optional>
#include <pqxx/row>
#include <string>

namespace vh::types {
struct User {
    unsigned short id;
    std::string name;
    std::string email;
    std::string password_hash;
    std::time_t created_at;
    std::optional<std::time_t> last_login;
    bool is_active;

    User() = default;

    User(std::string name, std::string email, const bool isActive)
        : id(0), name(std::move(name)), email(std::move(email)), is_active(isActive) {
        created_at = std::time(nullptr);
        last_login = std::nullopt;
    }

    explicit User(const pqxx::row& row)
        : id(row["id"].as<unsigned short>()), name(row["name"].as<std::string>()),
          email(row["email"].as<std::string>()), password_hash(row["password_hash"].as<std::string>()),
          created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
          last_login(row["last_login"].is_null()
                         ? std::nullopt
                         : std::make_optional(util::parsePostgresTimestamp(row["last_login"].as<std::string>()))),
          is_active(row["is_active"].as<bool>()) {}

    void setPasswordHash(const std::string& hash) { password_hash = hash; }

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"id", id},
            {"name", name},
            {"email", email},
            {"created_at", util::timestampToString(created_at)},
            {"last_login", last_login.has_value() ? util::timestampToString(last_login.value()) : ""},
            {"is_active", is_active},
        };
    }
};

BOOST_DESCRIBE_STRUCT(types::User, (), (id, name, email, password_hash, created_at, last_login, is_active))

inline void to_json(nlohmann::json& j, const User& u) {
    j = nlohmann::json{
        {"id", u.id},
        {"name", u.name},
        {"email", u.email},
        {"last_login", u.last_login},
    };
}

inline void from_json(const nlohmann::json& j, User& u) {
    j.at("name").get_to(u.name);
    j.at("email").get_to(u.email);
}

inline nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& user : users) j.push_back(user->toJson());
    return j;
}

inline nlohmann::json to_json(const std::vector<std::pair<std::shared_ptr<User>, std::string>>& users) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& [user, role] : users) {
        auto data = user->toJson();
        data["role"] = role;
        j.push_back(data);
    }
    return j;
}

inline nlohmann::json to_json(const std::pair<std::shared_ptr<User>, std::string>& userWithRole) {
    nlohmann::json j = userWithRole.first->toJson();
    j["role"] = userWithRole.second;
    return j;
}

} // namespace vh::types
