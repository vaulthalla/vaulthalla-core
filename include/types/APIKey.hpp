#pragma once

#include "util/timestamp.hpp"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>
#include <ctime>

namespace pqxx {
    class row;
    class result;
}

namespace vh::types::api {

enum class S3Provider {
    AWS, CloudflareR2, Wasabi, BackblazeB2,
    DigitalOcean, MinIO, Ceph, Storj, Other
};

std::string to_string(S3Provider provider);
S3Provider s3_provider_from_string(const std::string& str);

struct APIKey {
    unsigned int id{0};
    unsigned int user_id{0};
    std::string name;
    std::time_t created_at{std::time(nullptr)};

    // S3 standard metadata
    S3Provider provider{S3Provider::AWS};
    std::string access_key;
    std::string region;
    std::string endpoint;

    // Encrypted parts (stored in DB)
    std::vector<uint8_t> encrypted_secret_access_key, iv;

    // Runtime only (never stored in DB)
    std::string secret_access_key;

    APIKey() = default;
    APIKey(unsigned int userId, std::string name,
           S3Provider provider, std::string accessKey,
           std::string secretAccessKey,
           std::string region, std::string endpoint);

    explicit APIKey(const pqxx::row& row);
};

// JSON
void to_json(nlohmann::json& j, const APIKey& k);
void to_json(nlohmann::json& j, const std::shared_ptr<APIKey>& k);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<APIKey>>& k);
void from_json(const nlohmann::json& j, APIKey& key);

std::vector<std::shared_ptr<APIKey>> api_keys_from_pq_res(const pqxx::result& res);

std::string to_string(const std::shared_ptr<APIKey>& key);
std::string to_string(const std::vector<std::shared_ptr<APIKey>>& keys);

} // namespace vh::types::api
