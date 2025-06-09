#pragma once

#include <boost/describe.hpp>
#include <ctime>

namespace vh::types {
    struct UserStorageUsage {
        unsigned int user_id;
        unsigned int storage_volume_id;
        unsigned long long total_bytes;
        unsigned long long used_bytes;
        std::time_t updated_at;
    };
}

BOOST_DESCRIBE_STRUCT(vh::types::UserStorageUsage, (), (user_id, storage_volume_id, total_bytes, used_bytes, updated_at))
