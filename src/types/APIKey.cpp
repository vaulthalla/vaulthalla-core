#include "types/APIKey.hpp"

#include <unordered_map>
#include <stdexcept>
#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types::api;

// --- S3Provider helpers ---
std::string vh::types::api::to_string(S3Provider provider) {
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

S3Provider vh::types::api::s3_provider_from_string(const std::string& str) {
    static const std::unordered_map<std::string, S3Provider> mapping = {
        {"AWS", S3Provider::AWS}, {"Cloudflare R2", S3Provider::CloudflareR2},
        {"Wasabi", S3Provider::Wasabi}, {"Backblaze B2", S3Provider::BackblazeB2},
        {"DigitalOcean", S3Provider::DigitalOcean}, {"MinIO", S3Provider::MinIO},
        {"Ceph", S3Provider::Ceph}, {"Storj", S3Provider::Storj},
        {"Other", S3Provider::Other}
    };
    auto it = mapping.find(str);
    if (it != mapping.end()) return it->second;
    throw std::invalid_argument("Invalid S3Provider string: " + str);
}

// --- APIKey constructors ---
APIKey::APIKey(const unsigned int userId, std::string name,
               const S3Provider provider, std::string accessKey,
               std::string secretAccessKey,
               std::string region, std::string endpoint)
    : user_id(userId),
      name(std::move(name)),
      provider(provider),
      access_key(std::move(accessKey)),
      secret_access_key(std::move(secretAccessKey)), // runtime only, will be encrypted before insert
      region(std::move(region)),
      endpoint(std::move(endpoint)) {}

APIKey::APIKey(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      user_id(row["user_id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      provider(s3_provider_from_string(row["provider"].as<std::string>())),
      access_key(row["access_key"].as<std::string>()),
      region(row["region"].as<std::string>()),
      endpoint(row["endpoint"].as<std::string>())
{
    // Encrypted secret_access_key
    if (!row["encrypted_secret_access_key"].is_null()) {
        auto blob = row["encrypted_secret_access_key"].as<std::string>();
        encrypted_secret_access_key.assign(blob.begin(), blob.end());
    }

    // IV for AES-GCM
    if (!row["iv"].is_null()) {
        auto blob = row["iv"].as<std::string>();
        iv.assign(blob.begin(), blob.end());
    }

    // secret_access_key stays empty until APIKeyManager decrypts it
}

void vh::types::api::from_json(const nlohmann::json& j, APIKey& key) {
    key.id = j.at("api_key_id").get<unsigned int>();
    key.user_id = j.at("user_id").get<unsigned int>();
    key.name = j.at("name").get<std::string>();
    key.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
    key.provider = s3_provider_from_string(j.at("provider").get<std::string>());
    key.access_key = j.at("access_key").get<std::string>();
    key.region = j.at("region").get<std::string>();
    key.endpoint = j.at("endpoint").get<std::string>();
    // secret_access_key intentionally not parsed (comes from decryption at runtime)
}

std::vector<std::shared_ptr<APIKey>> vh::types::api::api_keys_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<APIKey>> keys;
    keys.reserve(res.size());
    for (const auto& row : res) keys.push_back(std::make_shared<APIKey>(row));
    return keys;
}

void vh::types::api::to_json(nlohmann::json& j, const APIKey& k) {
    j = nlohmann::json{
            {"api_key_id", k.id},
            {"user_id", k.user_id},
            {"name", k.name},
            {"created_at", util::timestampToString(k.created_at)},
            {"provider", to_string(k.provider)},
            {"access_key", k.access_key},
            {"region", k.region},
            {"endpoint", k.endpoint},
            // Encrypted fields are not included in public JSON representation
    };
}

void vh::types::api::to_json(nlohmann::json& j, const std::shared_ptr<APIKey>& k) {
    to_json(j, *k);
}

void vh::types::api::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<APIKey>>& k) {
    j = nlohmann::json::array();
    for (const auto& key : k) j.push_back(key);
}
