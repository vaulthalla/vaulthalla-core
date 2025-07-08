#include "types/Vault.hpp"
#include "shared_util/timestamp.hpp"
#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <utility>

namespace vh::types {

std::string to_string(const VaultType type) {
    switch (type) {
        case VaultType::Local: return "local";
        case VaultType::S3: return "s3";
        default: return "unknown";
    }
}

VaultType from_string(const std::string& type) {
    if (type == "local") return VaultType::Local;
    if (type == "s3") return VaultType::S3;
    throw std::invalid_argument("Invalid VaultType: " + type);
}

// ───── Vault ─────
Vault::Vault(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      type(from_string(row["type"].as<std::string>())),
      is_active(row["is_active"].as<bool>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].c_str())) {}

// ───── LocalDiskVault ─────
LocalDiskVault::LocalDiskVault(const std::string& name, std::filesystem::path mountPoint)
    : Vault(), mount_point(std::move(mountPoint)) {
    this->name = name;
    this->type = VaultType::Local;
    this->is_active = true;
    this->created_at = std::time(nullptr);
}

LocalDiskVault::LocalDiskVault(const pqxx::row& row)
    : Vault(row),
      vault_id(row["vault_id"].as<unsigned int>()),
      mount_point(std::filesystem::path(row["mount_point"].as<std::string>())) {}

// ───── S3Vault ─────
S3Vault::S3Vault(const std::string& name, unsigned short apiKeyID, std::string bucketName)
    : Vault(), api_key_id(apiKeyID), bucket(std::move(bucketName)) {
    this->name = name;
    this->type = VaultType::S3;
    this->is_active = true;
    this->created_at = std::time(nullptr);
}

S3Vault::S3Vault(const pqxx::row& row)
    : Vault(row),
      vault_id(row["vault_id"].as<unsigned short>()),
      api_key_id(row["api_key_id"].as<unsigned short>()),
      bucket(row["bucket"].as<std::string>()) {}

// ───── JSON ─────
void to_json(nlohmann::json& j, const Vault& v) {
    j = {
        {"id", v.id},
        {"name", v.name},
        {"type", to_string(v.type)},
        {"description", v.description},
        {"owner_id", v.owner_id},
        {"is_active", v.is_active},
        {"created_at", util::timestampToString(v.created_at)}
    };
}

void from_json(const nlohmann::json& j, Vault& v) {
    v.id = j.at("id").get<unsigned int>();
    v.name = j.at("name").get<std::string>();
    v.description = j.at("description").get<std::string>();
    v.type = from_string(j.at("type").get<std::string>());
    v.is_active = j.at("is_active").get<bool>();
    v.created_at = util::parseTimestampFromString(j.at("created_at").get<std::string>());
}

void to_json(nlohmann::json& j, const LocalDiskVault& v) {
    to_json(j, static_cast<const Vault&>(v));
    j["vault_id"] = v.vault_id;
    j["mount_point"] = v.mount_point.string();
}

void from_json(const nlohmann::json& j, LocalDiskVault& v) {
    from_json(j, static_cast<Vault&>(v));
    v.vault_id = j.at("vault_id").get<unsigned int>();
    v.mount_point = j.at("mount_point").get<std::string>();
}

void to_json(nlohmann::json& j, const S3Vault& v) {
    to_json(j, static_cast<const Vault&>(v));
    j["vault_id"] = v.vault_id;
    j["api_key_id"] = v.api_key_id;
    j["bucket"] = v.bucket;
}

void from_json(const nlohmann::json& j, S3Vault& v) {
    from_json(j, static_cast<Vault&>(v));
    v.vault_id = j.at("vault_id").get<unsigned short>();
    v.api_key_id = j.at("api_key_id").get<unsigned short>();
    v.bucket = j.at("bucket").get<std::string>();
}

nlohmann::json to_json(const std::vector<std::shared_ptr<Vault>>& vaults) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& vault : vaults) {
        if (const auto* local = dynamic_cast<const LocalDiskVault*>(vault.get())) {
            j.push_back(*local);
        } else if (const auto* s3 = dynamic_cast<const S3Vault*>(vault.get())) {
            j.push_back(*s3);
        } else {
            j.push_back(*vault);
        }
    }
    return j;
}

} // namespace vh::types
