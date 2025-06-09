#pragma once

#include <string>
#include <boost/describe.hpp>

namespace vh::types {
    struct UserRole {
        unsigned int user_id;
        unsigned int role_id;
    };
}

BOOST_DESCRIBE_STRUCT(vh::types::UserRole, (),
                      (user_id, role_id))
