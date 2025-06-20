#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <pqxx/row>
#include <string>

namespace vh::types {

struct UserStorageVolume {
    unsigned int user_id;
    unsigned int storage_volume_id;

    UserStorageVolume() = default;

    UserStorageVolume(const pqxx::row& row)
        : user_id(row["user_id"].as<unsigned int>()), storage_volume_id(row["storage_volume_id"].as<unsigned int>()) {}
};

struct UserStorageUsage {
    unsigned int user_id;
    unsigned int storage_volume_id;
    unsigned long long total_bytes;
    unsigned long long used_bytes;
    std::time_t updated_at;
};

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::UserStorageUsage, (),
                      (user_id, storage_volume_id, total_bytes, used_bytes, updated_at))

BOOST_DESCRIBE_STRUCT(vh::types::UserStorageVolume, (), (user_id, storage_volume_id))
