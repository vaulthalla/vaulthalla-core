#pragma once

#include "FilePermission.hpp"

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>

namespace vh::types {

    struct FileShare {
        unsigned int id;
        unsigned int file_id;
        unsigned int shared_by;
        std::optional<std::time_t> expires_at;
        std::time_t created_at;
    };

    struct FileShareUser {
        unsigned int id;
        unsigned int file_share_id;
        unsigned int shared_with_user_id;
        FilePermission permissions;
    };

    struct PublicFileShare {
        unsigned int id;
        unsigned int file_id;
        unsigned int shared_by;
        std::string share_token;
        FilePermission permissions;
        std::optional<std::time_t> expires_at;
        std::time_t created_at;
    };

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::FileShare, (), (id, file_id, shared_by, expires_at, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::FileShareUser, (), (id, file_share_id, shared_with_user_id, permissions))

BOOST_DESCRIBE_STRUCT(vh::types::PublicFileShare, (),
                      (id, file_id, shared_by, share_token, permissions, expires_at, created_at))
