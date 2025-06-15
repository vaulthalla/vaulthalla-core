#pragma once

#include <string>
#include <ctime>
#include <boost/describe.hpp>
#include <pqxx/row>
#include <nlohmann/json.hpp>
#include <optional>

namespace vh::types {
    struct User {
        unsigned short id;
        std::string name;
        std::string email;
        std::string password_hash;
        std::time_t created_at;
        std::optional<std::time_t> last_login;
        bool is_active;

        explicit User(const pqxx::row& row)
            : id(row["id"].as<unsigned short>()),
              name(row["name"].as<std::string>()),
              email(row["email"].as<std::string>()),
              password_hash(row["password_hash"].as<std::string>()),
              created_at(row["created_at"].as<std::time_t>()),
              last_login(row["last_login"].as<std::time_t>()),
              is_active(row["is_active"].as<bool>()) {}

        void setPasswordHash(const std::string& hash) { password_hash = hash; }
    };

    BOOST_DESCRIBE_STRUCT(vh::types::User, (),
                          (id, name, email, password_hash, created_at, last_login, is_active))

    inline void to_json(nlohmann::json& j, const User& u) {
        j = nlohmann::json{
                {"id", u.id},
                {"name", u.name},
                {"email", u.email},
                {"lastLogin", u.last_login},
        };
    }

    inline void from_json(const nlohmann::json& j, User& u) {
        j.at("name").get_to(u.name);
        j.at("email").get_to(u.email);
    }
}
