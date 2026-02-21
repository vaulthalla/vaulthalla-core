#pragma once

#include "types/sync/Policy.hpp"
#include "types/sync/Action.hpp"

#include <string>
#include <optional>
#include <nlohmann/json_fwd.hpp>
#include <memory>

namespace pqxx {
class row;
}

namespace vh::concurrency {
struct SyncTask;
}

namespace vh::types {
struct File;
}

namespace vh::types::sync {

struct RemotePolicy final : public Policy {
    enum class Strategy { Cache, Sync, Mirror };

    enum class ConflictPolicy {
        KeepLocal,
        KeepRemote,
        KeepNewest,
        Ask
    };

    Strategy strategy{Strategy::Cache};
    ConflictPolicy conflict_policy{ConflictPolicy::KeepLocal};

    RemotePolicy() = default;
    ~RemotePolicy() override = default;
    explicit RemotePolicy(const pqxx::row& row);

    void rehash_config() override;
    [[nodiscard]] bool resolve_conflict(const std::shared_ptr<Conflict>& conflict) const override;

    [[nodiscard]] bool wantsEnsureDirectories() const;
    [[nodiscard]] bool downloadRemoteOnly() const;
    [[nodiscard]] bool uploadLocalOnly() const;
    [[nodiscard]] bool deleteRemoteLeftovers() const;
    [[nodiscard]] bool deleteLocalLeftovers() const;
    [[nodiscard]] std::optional<ActionType> decideForBoth(const std::shared_ptr<types::File>& L, const std::shared_ptr<types::File>& R) const;
    void preflightSpaceForPlan(const std::weak_ptr<concurrency::SyncTask>& ctx, const std::vector<Action>& plan) const;
};

void to_json(nlohmann::json& j, const RemotePolicy& s);
void from_json(const nlohmann::json& j, RemotePolicy& s);

std::string to_string(const RemotePolicy::Strategy& s);
std::string to_string(const RemotePolicy::ConflictPolicy& cp);

RemotePolicy::Strategy strategyFromString(const std::string& str);
RemotePolicy::ConflictPolicy rsConflictPolicyFromString(const std::string& str);

std::string to_string(const std::shared_ptr<RemotePolicy>& sync);

}
