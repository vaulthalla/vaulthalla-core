#include "types/db/Volume.hpp"
#include "util/timestamp.hpp"
#include <pqxx/row>
#include <nlohmann/json.hpp>

namespace vh::types {

Volume::Volume(unsigned int vaultId, std::string name, std::filesystem::path pathPrefix,
               std::optional<unsigned long long> quotaBytes)
                   : vault_id(vaultId), name(std::move(name)),
                     path_prefix(std::move(pathPrefix)), quota_bytes(std::move(quotaBytes)) {}

Volume::Volume(const pqxx::row& row)
: id(row["id"].as<unsigned int>()), vault_id(row["vault_id"].as<unsigned int>()),
          name(row["name"].as<std::string>()), path_prefix(row["path_prefix"].as<std::string>()),
          quota_bytes(row["quota_bytes"].is_null() ? std::nullopt
                                                   : std::make_optional(row["quota_bytes"].as<unsigned long long>())),
          created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {}


void to_json(nlohmann::json& j, const Volume& v) {
    j = nlohmann::json{{"id", v.id},
                       {"vault_id", v.vault_id},
                       {"name", v.name},
                       {"path_prefix", v.path_prefix.string()},
                       {"quota_bytes", v.quota_bytes}};

    if (v.created_at) j["created_at"] = util::timestampToString(v.created_at);
    else j["created_at"] = util::timestampToString(std::time(nullptr));
}

void from_json(const nlohmann::json& j, Volume& v) {
    v.id = j.at("id").get<unsigned int>();
    v.vault_id = j.at("vault_id").get<unsigned int>();
    v.name = j.at("name").get<std::string>();
    v.path_prefix = j.at("path_prefix").get<std::string>();
    v.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());

    if (j.contains("quota_bytes")) v.quota_bytes = j.at("quota_bytes").get<unsigned long long>();
    else v.quota_bytes = std::nullopt;
}

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Volume>>& volumes) {
    j = nlohmann::json::array();
    for (const auto& volume : volumes) j.push_back(*volume);
}

}

