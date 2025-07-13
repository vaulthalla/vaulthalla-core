#include "types/S3Bucket.hpp"
#include "types/APIKey.hpp"
#include "shared_util/timestamp.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types::api;

S3Bucket::S3Bucket(const pqxx::row& row)
    : api_key(std::make_unique<S3APIKey>(row)),
      name(row.at("name").as<std::string>()),
      created_at(row.at("created_at").as<std::time_t>()),
      updated_at(row.at("updated_at").as<std::time_t>()),
      enabled(row.at("enabled").as<bool>()) {}

void vh::types::api::to_json(nlohmann::json& j, const S3Bucket& bucket) {
    j = {
            {"api_key", *bucket.api_key},
            {"name", bucket.name},
            {"created_at", util::timestampToString(bucket.created_at)},
            {"updated_at", util::timestampToString(bucket.updated_at)},
            {"enabled", bucket.enabled}
    };
}

void vh::types::api::from_json(const nlohmann::json& j, S3Bucket& bucket) {
    bucket.api_key = std::make_unique<S3APIKey>(j.at("api_key"));
    bucket.name = j.at("name").get<std::string>();
    bucket.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
    bucket.updated_at = util::parsePostgresTimestamp(j.at("updated_at").get<std::string>());
    bucket.enabled = j.at("enabled").get<bool>();
}
