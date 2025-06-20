#pragma once

#include "Vault.hpp"
#include "util/timestamp.hpp"
#include <boost/describe.hpp>
#include <ctime>
#include <nlohmann/json.hpp>
#include <optional>
#include <pqxx/row>
#include <string>
#include <utility>

namespace vh::types {

struct StorageVolume {
    unsigned int id{0};
    unsigned int vault_id{0};
    std::string name;
    std::filesystem::path path_prefix;
    std::optional<unsigned long long> quota_bytes;
    std::time_t created_at{std::time(nullptr)};

    StorageVolume() = default;

    StorageVolume(unsigned int vaultId, std::string name, std::filesystem::path pathPrefix,
                  std::optional<unsigned long long> quotaBytes = std::nullopt)
        : id(0), // ID will be set by the database
          vault_id(vaultId), name(std::move(name)), path_prefix(std::move(pathPrefix)), quota_bytes(quotaBytes),
          created_at(std::time(nullptr)) {} // Set current time

    explicit StorageVolume(const pqxx::row& row)
        : id(row["id"].as<unsigned int>()), vault_id(row["vault_id"].as<unsigned int>()),
          name(row["name"].as<std::string>()), path_prefix(row["path_prefix"].as<std::string>()),
          quota_bytes(row["quota_bytes"].is_null() ? std::nullopt
                                                   : std::make_optional(row["quota_bytes"].as<unsigned long long>())),
          created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {}

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StorageVolume, id, vault_id, name, path_prefix, quota_bytes, created_at)
};

inline nlohmann::json to_json(const std::vector<std::shared_ptr<StorageVolume>>& volumes) {
    nlohmann::json j;
    for (const auto& volume : volumes)
        if (volume) j.push_back(*volume);
    return j;
}

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::StorageVolume, (), (id, vault_id, name, path_prefix, quota_bytes, created_at))
