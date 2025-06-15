#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>
#include <pqxx/row>
#include <nlohmann/json.hpp>
#include <utility>

#include "util/timestamp.hpp"

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

    inline VaultType from_string(const std::string& type) {
        if (type == "local") return VaultType::Local;
        if (type == "s3") return VaultType::S3;
        throw std::invalid_argument("Invalid VaultType string: " + type);
    }

    struct Vault {
        unsigned int id{};
        std::string name;
        VaultType type{VaultType::Local};
        bool is_active{true};
        std::time_t created_at{};

        Vault() = default;
        virtual ~Vault() = default;

        explicit Vault(const pqxx::row& row)
            : id(row["id"].as<unsigned int>()),
              name(row["name"].as<std::string>()),
              type(from_string(row["type"].as<std::string>())),
              is_active(row["is_active"].as<bool>()),
              created_at(vh::util::parsePostgresTimestamp(row["created_at"].c_str())) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vault, id, name, type, is_active, created_at)
    };

    struct LocalDiskVault : public Vault {
        unsigned int vault_id{0};
        std::filesystem::path mount_point;

        LocalDiskVault() = default;

        LocalDiskVault(const std::string& name, std::filesystem::path mountPoint)
            : Vault(),
              vault_id(0), // This will be set by the database
              mount_point(std::move(mountPoint)) {
            this->name = name;
            this->type = VaultType::Local;
            this->is_active = true;
            this->created_at = std::time(nullptr); // Set current time
        }

        explicit LocalDiskVault(const pqxx::row& row)
            : Vault(row),
              vault_id(row["vault_id"].as<unsigned int>()),
              mount_point(row["mount_point"].as<std::string>()) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(LocalDiskVault, id, name, type, is_active, created_at, mount_point)
    };

    struct S3Vault : public Vault {
        unsigned short vault_id{0};
        unsigned short api_key_id{0};
        std::string bucket;

        S3Vault() = default;

        S3Vault(const std::string& name, unsigned short apiKeyID, std::string  bucketName)
            : Vault(),
              vault_id(0), // This will be set by the database
              api_key_id(apiKeyID),
              bucket(std::move(bucketName)) {
            this->name = name;
            this->type = VaultType::S3;
            this->is_active = true;
            this->created_at = std::time(nullptr); // Set current time
        }

        explicit S3Vault(const pqxx::row& row)
            : Vault(row),
              vault_id(row["vault_id"].as<unsigned short>()),
              api_key_id(row["api_key_id"].as<unsigned short>()),
              bucket(row["bucket"].as<std::string>()) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(S3Vault, id, name, type, is_active, created_at, api_key_id, bucket)
    };

    inline nlohmann::json to_json(const std::vector<std::shared_ptr<Vault>>& vaults) {
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
            if (const auto* diskVault = dynamic_cast<const LocalDiskVault*>(&*vault)) {
                vault_json["vault_id"] = diskVault->vault_id;
                vault_json["mount_point"] = diskVault->mount_point;
            }

                // Check for S3Vault fields
            else if (const auto* s3Vault = dynamic_cast<const S3Vault*>(&*vault)) {
                vault_json["vault_id"] = s3Vault->vault_id;
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

BOOST_DESCRIBE_STRUCT(vh::types::LocalDiskVault, (), (mount_point))

BOOST_DESCRIBE_STRUCT(vh::types::S3Vault, (), (id, api_key_id, bucket))
