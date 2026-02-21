#pragma once

#include "types/sync/Policy.hpp"

#include <string>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
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
};

void to_json(nlohmann::json& j, const RemotePolicy& s);
void from_json(const nlohmann::json& j, RemotePolicy& s);

std::string to_string(const RemotePolicy::Strategy& s);
std::string to_string(const RemotePolicy::ConflictPolicy& cp);

RemotePolicy::Strategy strategyFromString(const std::string& str);
RemotePolicy::ConflictPolicy rsConflictPolicyFromString(const std::string& str);

std::string to_string(const std::shared_ptr<RemotePolicy>& sync);

}
