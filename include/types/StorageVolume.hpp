#pragma once

#include "types/Vault.hpp"

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>
#include <pqxx/row>
#include <nlohmann/json.hpp>

namespace vh::types {

    struct StorageVolume {
        unsigned int id;
        unsigned int vault_id;
        std::string name;
        std::optional<std::string> path_prefix;
        std::optional<unsigned long long> quota_bytes;
        std::time_t created_at;

        StorageVolume() = default;

        StorageVolume(unsigned int vaultId, const std::string& name,
                      std::optional<std::string> pathPrefix = std::nullopt,
                      std::optional<unsigned long long> quotaBytes = std::nullopt)
            : id(0), // ID will be set by the database
              vault_id(vaultId),
              name(name),
              path_prefix(pathPrefix),
              quota_bytes(quotaBytes),
              created_at(std::time(nullptr)) {} // Set current time

        explicit StorageVolume(const pqxx::row& row)
            : id(row["id"].as<unsigned int>()),
              vault_id(row["vault_id"].as<unsigned int>()),
              name(row["name"].as<std::string>()),
              path_prefix(row["path_prefix"].is_null() ? std::nullopt : std::make_optional(row["path_prefix"].as<std::string>())),
              quota_bytes(row["quota_bytes"].is_null() ? std::nullopt : std::make_optional(row["quota_bytes"].as<unsigned long long>())),
              created_at(row["created_at"].as<std::time_t>()) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(StorageVolume,
                                       id,
                                       vault_id,
                                       name,
                                       path_prefix,
                                       quota_bytes,
                                       created_at)
    };

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::StorageVolume, (), (id, vault_id, name, path_prefix, quota_bytes, created_at))
