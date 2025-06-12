#pragma once

#include "types/StorageBackend.hpp"

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>
#include <pqxx/row>

namespace vh::types {

    struct StorageVolume {
        unsigned int id;
        unsigned int config_id;
        StorageBackendType type;
        std::string name;
        std::optional<std::string> path_prefix;
        std::optional<unsigned long long> quota_bytes;
        std::time_t created_at;

        explicit StorageVolume(const pqxx::row& row)
            : id(row["id"].as<unsigned int>()),
              config_id(row["config_id"].as<unsigned int>()),
              type(static_cast<StorageBackendType>(row["config_type"].as<int>())),
              name(row["name"].as<std::string>()),
              path_prefix(row["path_prefix"].is_null() ? std::nullopt : std::make_optional(row["path_prefix"].as<std::string>())),
              quota_bytes(row["quota_bytes"].is_null() ? std::nullopt : std::make_optional(row["quota_bytes"].as<unsigned long long>())),
              created_at(row["created_at"].as<std::time_t>()) {}
    };

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::StorageVolume, (), (id, config_id, type, name, path_prefix, quota_bytes, created_at))
