#pragma once

#include <boost/describe.hpp>
#include <string>

namespace vh::types {
enum class RoleName { Admin, User, Guest, Moderator, SuperAdmin };

struct Role {
    unsigned int id;
    RoleName name;
    std::string description;
};
} // namespace vh::types

BOOST_DESCRIBE_ENUM(vh::types::RoleName, Admin, User, Guest, Moderator, SuperAdmin)
BOOST_DESCRIBE_STRUCT(vh::types::Role, (), (id, name, description))
