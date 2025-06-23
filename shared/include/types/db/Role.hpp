#pragma once

#include <boost/describe.hpp>
#include <string>
#include <pqxx/row>

namespace vh::types {
enum class RoleName { Admin, User, Guest, Moderator, SuperAdmin };

inline std::string to_cli_string(const RoleName& role) {
    switch (role) {
        case RoleName::Admin: return "Administrator";
        case RoleName::User: return "User";
        case RoleName::Guest: return "Guest";
        case RoleName::Moderator: return "Moderator";
        case RoleName::SuperAdmin: return "Super Administrator";
        default: return "Unknown";
    }
}

inline std::string to_db_string(const RoleName& role) {
    switch (role) {
        case RoleName::Admin: return "Admin";
        case RoleName::User: return "User";
        case RoleName::Guest: return "Guest";
        case RoleName::Moderator: return "Moderator";
        case RoleName::SuperAdmin: return "SuperAdmin";
        default: throw std::invalid_argument("Unknown role name");
    }
}

inline RoleName role_from_db_string(const std::string& str) {
    if (str == "Admin") return RoleName::Admin;
    if (str == "User") return RoleName::User;
    if (str == "Guest") return RoleName::Guest;
    if (str == "Moderator") return RoleName::Moderator;
    if (str == "SuperAdmin") return RoleName::SuperAdmin;
    throw std::invalid_argument("Unknown role name: " + str);
}

inline std::string db_role_str_to_cli_str(const std::string& str) {
    if (str == "Admin") return "Administrator";
    if (str == "SuperAdmin") return "Super Administrator";
    return str;
}

inline std::string cli_role_str_to_db_string(const std::string& str) {
    if (str == "Administrator") return "Admin";
    if (str == "Super Administrator") return "SuperAdmin";
    return str;
}

inline RoleName role_from_cli_string(const std::string& str) {
    if (str == "Administrator") return RoleName::Admin;
    if (str == "User") return RoleName::User;
    if (str == "Guest") return RoleName::Guest;
    if (str == "Moderator") return RoleName::Moderator;
    if (str == "Super Administrator") return RoleName::SuperAdmin;
    throw std::invalid_argument("Unknown role name: " + str);
}

struct Role {
    unsigned int id;
    RoleName name;
    std::string description;

    Role() = default;

    explicit Role(const pqxx::row& row)
        : id(row["id"].as<unsigned int>()),
          name(role_from_db_string(row["name"].as<std::string>())),
          description(row["description"].as<std::string>()) {}
};
} // namespace vh::types

BOOST_DESCRIBE_ENUM(vh::types::RoleName, Admin, User, Guest, Moderator, SuperAdmin)
BOOST_DESCRIBE_STRUCT(vh::types::Role, (), (id, name, description))
