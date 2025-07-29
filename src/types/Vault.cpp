#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "util/timestamp.hpp"


#include <nlohmann/json.hpp>
#include <pqxx/row>

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

Vault::Vault(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      owner_id(row["owner_id"].as<unsigned int>()),
      quota(row["quota"].as<unsigned long long>()),
      type(from_string(row["type"].as<std::string>())),
      mount_point(std::filesystem::path(row["mount_point"].as<std::string>())),
      is_active(row["is_active"].as<bool>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].c_str())) {}

void to_json(nlohmann::json& j, const Vault& v) {
    j = {
        {"id", v.id},
        {"name", v.name},
        {"type", to_string(v.type)},
        {"description", v.description},
        {"quota", v.quota},
        {"owner_id", v.owner_id},
        {"mount_point", v.mount_point.string()},
        {"is_active", v.is_active},
        {"created_at", util::timestampToString(v.created_at)}
    };
}

void from_json(const nlohmann::json& j, Vault& v) {
    v.id = j.at("id").get<unsigned int>();
    v.name = j.at("name").get<std::string>();
    v.description = j.at("description").get<std::string>();
    v.quota = j.at("quota").get<unsigned long long>();
    v.type = from_string(j.at("type").get<std::string>());
    v.owner_id = j.at("owner_id").get<unsigned int>();
    v.mount_point = std::filesystem::path(j.at("mount_point").get<std::string>());
    v.is_active = j.at("is_active").get<bool>();
    v.created_at = util::parseTimestampFromString(j.at("created_at").get<std::string>());
}

nlohmann::json to_json(const std::vector<std::shared_ptr<Vault>>& vaults) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& vault : vaults) {
        if (const auto* s3 = dynamic_cast<const S3Vault*>(vault.get())) j.push_back(*s3);
        else j.push_back(*vault);
    }
    return j;
}

}
