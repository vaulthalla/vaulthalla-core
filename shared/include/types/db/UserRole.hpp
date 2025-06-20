#pragma once

#include <boost/describe.hpp>
#include <string>

namespace vh::types {
struct UserRole {
    unsigned int user_id;
    unsigned int role_id;
};
} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::UserRole, (), (user_id, role_id))
