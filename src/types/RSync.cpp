#include "types/RSync.hpp"
#include "util/timestamp.hpp"
#include "util/interval.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;
using namespace vh::util;

RSync::RSync(const pqxx::row& row)
    : Sync(row),
      strategy(strategyFromString(row.at("strategy").as<std::string>())),
      conflict_policy(rsConflictPolicyFromString(row.at("conflict_policy").as<std::string>())) {
}

void vh::types::to_json(nlohmann::json& j, const RSync& s) {
    to_json(j, static_cast<const Sync&>(s));
    j["strategy"] = to_string(s.strategy);
    j["conflict_policy"] = to_string(s.conflict_policy);
}

void vh::types::from_json(const nlohmann::json& j, RSync& s) {
    from_json(j, static_cast<Sync&>(s));
    s.strategy = strategyFromString(j.at("strategy").get<std::string>());
    s.conflict_policy = rsConflictPolicyFromString(j.at("conflict_policy").get<std::string>());
}

std::string vh::types::to_string(const RSync::Strategy& s) {
    switch (s) {
    case RSync::Strategy::Cache: return "cache";
    case RSync::Strategy::Sync: return "sync";
    case RSync::Strategy::Mirror: return "mirror";
    default: throw std::invalid_argument("Unknown sync strategy");
    }
}

std::string vh::types::to_string(const RSync::ConflictPolicy& cp) {
    switch (cp) {
    case RSync::ConflictPolicy::KeepLocal: return "keep_local";
    case RSync::ConflictPolicy::KeepRemote: return "keep_remote";
    case RSync::ConflictPolicy::Ask: return "ask";
    default: throw std::invalid_argument("Unknown conflict policy");
    }
}

RSync::Strategy vh::types::strategyFromString(const std::string& str) {
    if (str == "cache") return RSync::Strategy::Cache;
    if (str == "sync") return RSync::Strategy::Sync;
    if (str == "mirror") return RSync::Strategy::Mirror;
    throw std::invalid_argument("Unknown sync strategy: " + str);
}

RSync::ConflictPolicy vh::types::rsConflictPolicyFromString(const std::string& str) {
    if (str == "keep_local") return RSync::ConflictPolicy::KeepLocal;
    if (str == "keep_remote") return RSync::ConflictPolicy::KeepRemote;
    if (str == "ask") return RSync::ConflictPolicy::Ask;
    throw std::invalid_argument("Unknown conflict policy: " + str);
}

std::string vh::types::to_string(const std::shared_ptr<RSync>& sync) {
    if (!sync) return "null";
    return "Remote Vault Sync Configuration:\n"
           "  Vault ID: " + std::to_string(sync->vault_id) + "\n"
           "  Interval: " + intervalToString(sync->interval) + "\n"
           "  Enabled: " + (sync->enabled ? "true" : "false") + "\n"
           "  Strategy: " + to_string(sync->strategy) + "\n"
           "  Conflict Policy: " + to_string(sync->conflict_policy) + "\n"
           "  Last Sync At: " + timestampToString(sync->last_sync_at) + "\n"
           "  Last Success At: " + timestampToString(sync->last_success_at) + "\n"
           "  Created At: " + timestampToString(sync->created_at) + "\n"
           "  Updated At: " + timestampToString(sync->updated_at);
}
