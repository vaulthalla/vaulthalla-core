#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>
#include <pqxx/row>
#include <nlohmann/json.hpp>

namespace vh::types {
    enum class VaultType {
        Local,
        S3
    };

    inline std::string to_string(VaultType type) {
        switch (type) {
            case VaultType::Local: return "local";
            case VaultType::S3: return "s3";
            default: return "Unknown";
        }
    }

    struct Vault {
        unsigned int id;
        std::string name;
        VaultType type;
        bool is_active;
        std::time_t created_at;

        explicit Vault(const pqxx::row& row)
            : id(row["id"].as<unsigned int>()),
              name(row["name"].as<std::string>()),
              type(static_cast<VaultType>(row["type"].as<int>())),
              is_active(row["is_active"].as<bool>()),
              created_at(row["created_at"].as<std::time_t>()) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vault, id, name, type, is_active, created_at)
    };

    struct LocalDiskVault : public Vault {
        unsigned int storage_backend_id{0};
        std::string mount_point;

        LocalDiskVault() = default;

        LocalDiskVault(const std::string& name, const std::string& mountPoint)
            : Vault({}),
              storage_backend_id(0), // This will be set by the database
              mount_point(mountPoint) {
            this->name = name;
            this->type = VaultType::Local;
            this->is_active = true;
            this->created_at = std::time(nullptr); // Set current time
        }

        explicit LocalDiskVault(const pqxx::row& row)
            : Vault(row),
              storage_backend_id(row["storage_backend_id"].as<unsigned int>()),
              mount_point(row["mount_point"].as<std::string>()) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(LocalDiskVault, id, name, type, is_active, created_at, storage_backend_id, mount_point)
    };

    struct S3Vault : public Vault {
        unsigned short storage_backend_id;
        unsigned short api_key_id;
        std::string bucket;

        S3Vault() = default;

        S3Vault(const std::string& name, unsigned short apiKeyID, const std::string& bucketName)
            : Vault({}),
              storage_backend_id(0), // This will be set by the database
              api_key_id(apiKeyID),
              bucket(bucketName) {
            this->name = name;
            this->type = VaultType::S3;
            this->is_active = true;
            this->created_at = std::time(nullptr); // Set current time
        }

        explicit S3Vault(const pqxx::row& row)
            : Vault(row),
              storage_backend_id(row["storage_backend_id"].as<unsigned short>()),
              api_key_id(row["api_key_id"].as<unsigned short>()),
              bucket(row["bucket"].as<std::string>()) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(S3Vault, id, name, type, is_active, created_at, storage_backend_id, api_key_id, bucket)
    };
}

BOOST_DESCRIBE_ENUM(vh::types::VaultType, Local, S3)

BOOST_DESCRIBE_STRUCT(vh::types::Vault, (), (id, name, type, is_active, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::LocalDiskVault, (), (storage_backend_id, mount_point))

BOOST_DESCRIBE_STRUCT(vh::types::S3Vault, (), (id, storage_backend_id, api_key_id, bucket))
