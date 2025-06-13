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

        virtual ~Vault() = default;

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
        std::filesystem::path mount_point;

        LocalDiskVault() = default;

        LocalDiskVault(const std::string& name, const std::filesystem::path& mountPoint)
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

    inline nlohmann::json to_json(const std::vector<std::unique_ptr<Vault>>& vaults) {
        nlohmann::json j = nlohmann::json::array();

        for (const auto& vault : vaults) {
            nlohmann::json vault_json = {
                    {"id", vault->id},
                    {"name", vault->name},
                    {"type", to_string(vault->type)},
                    {"is_active", vault->is_active},
                    {"created_at", vault->created_at}
            };

            // Check for LocalDiskVault fields
            if (const auto* diskVault = dynamic_cast<const LocalDiskVault*>(vault.get())) {
                vault_json["storage_backend_id"] = diskVault->storage_backend_id;
                vault_json["mount_point"] = diskVault->mount_point;
            }

                // Check for S3Vault fields
            else if (const auto* s3Vault = dynamic_cast<const S3Vault*>(vault.get())) {
                vault_json["storage_backend_id"] = s3Vault->storage_backend_id;
                vault_json["api_key_id"] = s3Vault->api_key_id;
                vault_json["bucket"] = s3Vault->bucket;
            }

            j.push_back(vault_json);
        }

        return j;
    }
}

BOOST_DESCRIBE_ENUM(vh::types::VaultType, Local, S3)

BOOST_DESCRIBE_STRUCT(vh::types::Vault, (), (id, name, type, is_active, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::LocalDiskVault, (), (storage_backend_id, mount_point))

BOOST_DESCRIBE_STRUCT(vh::types::S3Vault, (), (id, storage_backend_id, api_key_id, bucket))
