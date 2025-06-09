#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>

namespace vh::types {
    enum class StorageBackendType {
        Local,
        S3
    };

    struct StorageBackend {
        unsigned int id;
        std::string name;
        StorageBackendType type;
        bool is_active;
        std::time_t created_at;
    };

    struct LocalDiskConfig {
        unsigned int storage_backend_id;
        std::string mount_point;
    };

    struct S3Config {
        unsigned int storage_backend_id;
        std::string bucket_name;
        std::string access_key;
        std::string secret_access_key;
        std::string region;
        std::optional<std::string> prefix;
        std::optional<std::string> endpoint;
    };
}

BOOST_DESCRIBE_ENUM(vh::types::StorageBackendType, Local, S3)

BOOST_DESCRIBE_STRUCT(vh::types::StorageBackend, (), (id, name, type, is_active, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::LocalDiskConfig, (), (storage_backend_id, mount_point))

BOOST_DESCRIBE_STRUCT(vh::types::S3Config, (),
                      (storage_backend_id, bucket_name, access_key, secret_access_key, region, prefix, endpoint))
