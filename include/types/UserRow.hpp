#pragma once

#include <string>
#include <ctime>
#include <boost/describe.hpp>

namespace vh::types {
    struct UserRow {
        int id;
        std::string username;
        std::string email;
        std::string password_hash;
        std::string display_name;
        std::time_t created_at;
        std::time_t last_login;
        bool is_active;
    };
}

BOOST_DESCRIBE_STRUCT(vh::types::UserRow, (),
                      (id, username, email, password_hash, display_name, created_at, last_login, is_active))
