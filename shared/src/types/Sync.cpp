#include "types/Sync.hpp"
#include "shared_util/timestamp.hpp"
#include "shared_util/interval.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

Sync::Sync(const pqxx::row& row)
    : id(row.at("id").as<unsigned int>()),
      interval(std::chrono::seconds(row.at("interval").as<uint64_t>())),
      conflict_policy(row.at("conflict_policy").as<std::string>()),
      strategy(row.at("strategy").as<std::string>()),
      enabled(row.at("enabled").as<bool>()),
      last_sync_at(row.at("last_sync_at").as<std::time_t>()),
      last_success_at(row.at("last_success_at").as<std::time_t>()),
      created_at(row.at("created_at").as<std::time_t>()),
      updated_at(row.at("updated_at").as<std::time_t>()) {
}

void vh::types::to_json(nlohmann::json& j, const Sync& s) {
    j = nlohmann::json{
        {"id", s.id},
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
    s.id = j.at("id").get<unsigned int>();
    s.interval = std::chrono::seconds(j.at("interval").get<uint64_t>());
    s.conflict_policy = j.at("conflict_policy").get<std::string>();
    s.strategy = j.at("strategy").get<std::string>();
    s.enabled = j.at("enabled").get<bool>();
    s.last_sync_at = util::parseTimestampFromString(j.at("last_sync_at").get<std::string>());
    s.last_success_at = util::parseTimestampFromString(j.at("last_success_at").get<std::string>());
    s.created_at = util::parseTimestampFromString(j.at("created_at").get<std::string>());
    s.updated_at = util::parseTimestampFromString(j.at("updated_at").get<std::string>());
}
