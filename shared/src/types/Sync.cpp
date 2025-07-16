#include "types/Sync.hpp"
#include "shared_util/timestamp.hpp"
#include "shared_util/interval.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

Sync::Sync(const pqxx::row& row)
    : id(row.at("id").as<unsigned int>()),
      vault_id(row.at("vault_id").as<unsigned int>()),
      interval(std::chrono::seconds(row.at("interval").as<uint64_t>())),
      conflict_policy(conflictPolicyFromString(row.at("conflict_policy").as<std::string>())),
      strategy(strategyFromString(row.at("strategy").as<std::string>())),
      enabled(row.at("enabled").as<bool>()),
      created_at(util::parsePostgresTimestamp(row.at("created_at").as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row.at("updated_at").as<std::string>())) {
    if (!row["last_sync_at"].is_null()) last_sync_at = util::parsePostgresTimestamp(row["last_sync_at"].as<std::string>());
    if (!row["last_success_at"].is_null()) last_success_at = util::parsePostgresTimestamp(row["last_success_at"].as<std::string>());
}

void vh::types::to_json(nlohmann::json& j, const Sync& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"vault_id", s.vault_id},
        {"interval", util::intervalToString(s.interval)},
        {"conflict_policy", s.conflict_policy},
        {"strategy", s.strategy},
        {"enabled", s.enabled},
        {"last_sync_at", util::timestampToString(s.last_sync_at)},
        {"last_success_at", util::timestampToString(s.last_success_at)},
        {"created_at", util::timestampToString(s.created_at)},
        {"updated_at", util::timestampToString(s.updated_at)}
    };
}

void vh::types::from_json(const nlohmann::json& j, Sync& s) {
    if (j.contains("id")) s.id = j.at("id").get<unsigned int>();
    if (j.contains("vault_id")) s.vault_id = j.at("vault_id").get<unsigned int>();
    s.interval = std::chrono::seconds(j.at("interval").get<uint64_t>());
    s.conflict_policy = conflictPolicyFromString(j.at("conflict_policy").get<std::string>());
    s.strategy = strategyFromString(j.at("strategy").get<std::string>());
    s.enabled = j.value("enabled", true);
    if (j.contains("last_sync_at")) s.last_sync_at = util::parseTimestampFromString(j.at("last_sync_at").get<std::string>());
    if (j.contains("last_success_at")) s.last_success_at = util::parseTimestampFromString(j.at("last_success_at").get<std::string>());
    if (j.contains("created_at")) s.created_at = util::parseTimestampFromString(j.at("created_at").get<std::string>());
    if (j.contains("updated_at")) s.updated_at = util::parseTimestampFromString(j.at("updated_at").get<std::string>());
}

void vh::types::to_string(std::string& str, const Sync::Strategy& s) {
    switch (s) {
        case Sync::Strategy::Cache: str = "Cache"; break;
        case Sync::Strategy::Sync: str = "Sync"; break;
        case Sync::Strategy::Mirror: str = "Mirror"; break;
    }
}

void vh::types::to_string(std::string& str, const Sync::ConflictPolicy& cp) {
    switch (cp) {
        case Sync::ConflictPolicy::KeepLocal: str = "KeepLocal"; break;
        case Sync::ConflictPolicy::KeepRemote: str = "KeepRemote"; break;
        case Sync::ConflictPolicy::Ask: str = "Ask"; break;
    }
}

Sync::Strategy vh::types::strategyFromString(const std::string& str) {
    if (str == "Cache") return Sync::Strategy::Cache;
    if (str == "Sync") return Sync::Strategy::Sync;
    if (str == "Mirror") return Sync::Strategy::Mirror;
    throw std::invalid_argument("Unknown sync strategy: " + str);
}

Sync::ConflictPolicy vh::types::conflictPolicyFromString(const std::string& str) {
    if (str == "KeepLocal") return Sync::ConflictPolicy::KeepLocal;
    if (str == "KeepRemote") return Sync::ConflictPolicy::KeepRemote;
    if (str == "Ask") return Sync::ConflictPolicy::Ask;
    throw std::invalid_argument("Unknown conflict policy: " + str);
}
