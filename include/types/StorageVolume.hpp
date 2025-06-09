#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>

namespace vh::types {

    struct StorageVolume {
        unsigned int id;
        unsigned int storage_backend_id;
        std::string name;
        std::optional<std::string> path_prefix;
        std::optional<unsigned long> quota_bytes;
        std::time_t created_at;
    };

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::StorageVolume, (),
                      (id, storage_backend_id, name, path_prefix, quota_bytes, created_at))
