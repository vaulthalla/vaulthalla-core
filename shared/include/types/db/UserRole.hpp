#pragma once

#include <boost/describe.hpp>
#include <pqxx/row>

namespace vh::types {
struct UserRole {
    unsigned int user_id;
    unsigned int role_id;

    UserRole() = default;

    UserRole(const pqxx::row& row)
        : user_id(row["user_id"].as<unsigned int>()), role_id(row["role_id"].as<unsigned int>()) {}
};
} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::UserRole, (), (user_id, role_id))
