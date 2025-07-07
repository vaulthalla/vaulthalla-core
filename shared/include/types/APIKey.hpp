#pragma once

#include "util/timestamp.hpp"
#include <boost/describe.hpp>
#include <ctime>
#include <nlohmann/json.hpp>
#include <optional>
#include <pqxx/row>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace vh::types::api {

enum class APIKeyType { S3 };

inline std::string to_string(APIKeyType type) {
    switch (type) {
    case APIKeyType::S3: return "s3";
    default: throw std::runtime_error("Unknown API key type");
    }
}

inline APIKeyType from_string(const std::string& type) {
    if (type == "s3") return APIKeyType::S3;
    throw std::runtime_error("Unknown API key type: " + type);
}

enum class S3Provider { AWS, CloudflareR2, Wasabi, BackblazeB2, DigitalOcean, MinIO, Ceph, Storj, Other };

inline std::string to_string(S3Provider provider) {
    switch (provider) {
    case S3Provider::AWS: return "AWS";
    case S3Provider::CloudflareR2: return "Cloudflare R2";
    case S3Provider::Wasabi: return "Wasabi";
    case S3Provider::BackblazeB2: return "Backblaze B2";
    case S3Provider::DigitalOcean: return "DigitalOcean";
    case S3Provider::MinIO: return "MinIO";
    case S3Provider::Ceph: return "Ceph";
    case S3Provider::Storj: return "Storj";
    case S3Provider::Other: return "Other";
    default: throw std::invalid_argument("Unknown S3Provider enum value");
    }
}

inline S3Provider s3_provider_from_string(const std::string& str) {
    static const std::unordered_map<std::string, S3Provider> mapping = {{"AWS", S3Provider::AWS},
                                                                        {"Cloudflare R2", S3Provider::CloudflareR2},
                                                                        {"Wasabi", S3Provider::Wasabi},
                                                                        {"Backblaze B2", S3Provider::BackblazeB2},
                                                                        {"DigitalOcean", S3Provider::DigitalOcean},
                                                                        {"MinIO", S3Provider::MinIO},
                                                                        {"Ceph", S3Provider::Ceph},
                                                                        {"Storj", S3Provider::Storj},
                                                                        {"Other", S3Provider::Other}};

    auto it = mapping.find(str);
    if (it != mapping.end()) return it->second;
    throw std::invalid_argument("Invalid S3Provider string: " + str);
}

struct APIKey {
    unsigned int id{0};
    unsigned int user_id{0};
    APIKeyType type{APIKeyType::S3};
    std::string name;
    std::time_t created_at{std::time(nullptr)};

    std::optional<S3Provider> provider; // only present for S3 metadata

    APIKey() = default;
    virtual ~APIKey() = default;

    APIKey(unsigned int userId, const APIKeyType& type, std::string name)
        : user_id(userId), type(type), name(std::move(name)) {}

    explicit APIKey(const pqxx::row& row)
        : id(row["id"].as<unsigned int>()), user_id(row["user_id"].as<unsigned int>()),
          type(from_string(row["type"].as<std::string>())), name(row["name"].as<std::string>()),
          created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {
        // Optional provider (only for S3 keys)
        if (type == APIKeyType::S3 && !row["provider"].is_null()) {
            provider = s3_provider_from_string(row["provider"].as<std::string>());
        }
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(APIKey, id, user_id, type, name, created_at, provider)
};

struct S3APIKey : public APIKey {
    S3Provider provider{S3Provider::AWS};
    std::string access_key;
    std::string secret_access_key;
    std::string region;
    std::string endpoint;

    S3APIKey() = default;

    S3APIKey(const std::string& name, unsigned int userId, const S3Provider& provider, std::string accessKey,
             std::string secretAccessKey, std::string region, std::string endpoint)
        : APIKey{userId, APIKeyType::S3, name}, provider(provider), access_key(std::move(accessKey)),
          secret_access_key(std::move(secretAccessKey)), region(std::move(region)), endpoint(std::move(endpoint)) {}

    explicit S3APIKey(const pqxx::row& row)
        : APIKey(row), provider(s3_provider_from_string(row["provider"].as<std::string>())),
          access_key(row["access_key"].as<std::string>()),
          secret_access_key(row["secret_access_key"].as<std::string>()), region(row["region"].as<std::string>()),
          endpoint(row["endpoint"].as<std::string>()) {}

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(vh::types::api::S3APIKey, id, user_id, type, name, created_at, provider, access_key,
                                   secret_access_key, region, endpoint)
};

inline nlohmann::json to_json(const std::shared_ptr<APIKey>& key) {
    nlohmann::json key_json = {{"api_key_id", key->id},
                               {"user_id", key->user_id},
                               {"type", to_string(key->type)},
                               {"name", key->name},
                               {"created_at", util::timestampToString(key->created_at)}};

    if (key->provider.has_value()) {
        key_json["provider"] = to_string(key->provider.value());
    }

    return key_json;
}

inline nlohmann::json to_json(const std::vector<std::shared_ptr<APIKey>>& keys) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& key : keys) j.push_back(to_json(key));
    return j;
}

} // namespace vh::types::api

BOOST_DESCRIBE_STRUCT(vh::types::api::APIKey, (), (id, user_id, name, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::api::S3APIKey, (vh::types::api::APIKey),
                      (access_key, secret_access_key, region, endpoint))
