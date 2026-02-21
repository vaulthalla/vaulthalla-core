#include "types/sync/RemotePolicy.hpp"
#include "util/timestamp.hpp"
#include "util/interval.hpp"
#include "types/sync/Conflict.hpp"
#include "types/fs/File.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types::sync;
using namespace vh::util;

RemotePolicy::RemotePolicy(const pqxx::row& row)
    : Policy(row),
      strategy(strategyFromString(row.at("strategy").as<std::string>())),
      conflict_policy(rsConflictPolicyFromString(row.at("conflict_policy").as<std::string>())) {
    rehash_config();
}

void RemotePolicy::rehash_config() {
    config_hash = "vault_id=" + std::to_string(vault_id) +
                  ";interval=" + std::to_string(interval.count()) +
                  ";enabled=" + (enabled ? "true" : "false") +
                  ";strategy=" + to_string(strategy) +
                  ";conflict_policy=" + to_string(conflict_policy);
}

bool RemotePolicy::resolve_conflict(const std::shared_ptr<Conflict>& conflict) const {
    if (conflict_policy == ConflictPolicy::Ask) return false;

    if (conflict_policy == ConflictPolicy::KeepLocal) conflict->resolution = Conflict::Resolution::KEPT_LOCAL;
    else if (conflict_policy == ConflictPolicy::KeepRemote) conflict->resolution = Conflict::Resolution::KEPT_REMOTE;
    else if (conflict_policy == ConflictPolicy::KeepNewest) {
        if (conflict->artifacts.local.file->updated_at == std::time_t{0} || conflict->artifacts.upstream.file->updated_at == std::time_t{0})
            return false; // If either timestamp is missing, we can't determine which is newer, so we ask the user.

        if (conflict->artifacts.local.file->updated_at > conflict->artifacts.upstream.file->updated_at) conflict->resolution = Conflict::Resolution::KEPT_LOCAL;
        else conflict->resolution = Conflict::Resolution::KEPT_REMOTE;
    }

    return true;
}

void vh::types::sync::to_json(nlohmann::json& j, const RemotePolicy& s) {
    to_json(j, static_cast<const Policy&>(s));
    j["strategy"] = to_string(s.strategy);
    j["conflict_policy"] = to_string(s.conflict_policy);
}

void vh::types::sync::from_json(const nlohmann::json& j, RemotePolicy& s) {
    from_json(j, static_cast<Policy&>(s));
    s.strategy = strategyFromString(j.at("strategy").get<std::string>());
    s.conflict_policy = rsConflictPolicyFromString(j.at("conflict_policy").get<std::string>());
}

std::string vh::types::sync::to_string(const RemotePolicy::Strategy& s) {
    switch (s) {
    case RemotePolicy::Strategy::Cache: return "cache";
    case RemotePolicy::Strategy::Sync: return "sync";
    case RemotePolicy::Strategy::Mirror: return "mirror";
    default: throw std::invalid_argument("Unknown sync strategy");
    }
}

std::string vh::types::sync::to_string(const RemotePolicy::ConflictPolicy& cp) {
    switch (cp) {
    case RemotePolicy::ConflictPolicy::KeepLocal: return "keep_local";
    case RemotePolicy::ConflictPolicy::KeepRemote: return "keep_remote";
    case RemotePolicy::ConflictPolicy::KeepNewest: return "keep_newest";
    case RemotePolicy::ConflictPolicy::Ask: return "ask";
    default: throw std::invalid_argument("Unknown conflict policy");
    }
}

RemotePolicy::Strategy vh::types::sync::strategyFromString(const std::string& str) {
    if (str == "cache") return RemotePolicy::Strategy::Cache;
    if (str == "sync") return RemotePolicy::Strategy::Sync;
    if (str == "mirror") return RemotePolicy::Strategy::Mirror;
    throw std::invalid_argument("Unknown sync strategy: " + str);
}

RemotePolicy::ConflictPolicy vh::types::sync::rsConflictPolicyFromString(const std::string& str) {
    if (str == "keep_local") return RemotePolicy::ConflictPolicy::KeepLocal;
    if (str == "keep_remote") return RemotePolicy::ConflictPolicy::KeepRemote;
    if (str == "keep_newest") return RemotePolicy::ConflictPolicy::KeepNewest;
    if (str == "ask") return RemotePolicy::ConflictPolicy::Ask;
    throw std::invalid_argument("Unknown conflict policy: " + str);
}

std::string vh::types::sync::to_string(const std::shared_ptr<RemotePolicy>& sync) {
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
