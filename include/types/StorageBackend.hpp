#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>
#include <pqxx/row>

namespace vh::types {
    enum class StorageBackendType {
        Local,
        S3
    };

    inline std::string to_string(StorageBackendType type) {
        switch (type) {
            case StorageBackendType::Local: return "local";
            case StorageBackendType::S3: return "s3";
            default: return "Unknown";
        }
    }

    struct StorageBackend {
        unsigned int id;
        std::string name;
        StorageBackendType type;
        bool is_active;
        std::time_t created_at;

        explicit StorageBackend(const pqxx::row& row)
            : id(row["id"].as<unsigned int>()),
              name(row["name"].as<std::string>()),
              type(static_cast<StorageBackendType>(row["type"].as<int>())),
              is_active(row["is_active"].as<bool>()),
              created_at(row["created_at"].as<std::time_t>()) {}
    };

    struct LocalDiskConfig : public StorageBackend {
        unsigned int storage_backend_id;
        std::string mount_point;

        explicit LocalDiskConfig(const pqxx::row& row)
            : StorageBackend(row),
              storage_backend_id(row["storage_backend_id"].as<unsigned int>()),
              mount_point(row["mount_point"].as<std::string>()) {}
    };

    struct S3Config : public StorageBackend {
        unsigned short storage_backend_id;
        unsigned short api_key_id;
        std::string bucket;


        explicit S3Config(const pqxx::row& row)
            : StorageBackend(row),
              storage_backend_id(row["storage_backend_id"].as<unsigned short>()),
              api_key_id(row["api_key_id"].as<unsigned short>()),
              bucket(row["bucket"].as<std::string>()) {}
    };
}

BOOST_DESCRIBE_ENUM(vh::types::StorageBackendType, Local, S3)

BOOST_DESCRIBE_STRUCT(vh::types::StorageBackend, (), (id, name, type, is_active, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::LocalDiskConfig, (), (storage_backend_id, mount_point))

BOOST_DESCRIBE_STRUCT(vh::types::S3Config, (), (id, storage_backend_id, api_key_id, bucket))
