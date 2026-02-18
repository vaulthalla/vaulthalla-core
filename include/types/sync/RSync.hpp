#pragma once

#include "types/sync/Sync.hpp"

#include <string>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
}

namespace vh::types {

struct RSync final : public Sync {
    enum class Strategy { Cache, Sync, Mirror };

    enum class ConflictPolicy {
        KeepLocal,
        KeepRemote,
        Ask
    };

    Strategy strategy{Strategy::Cache};
    ConflictPolicy conflict_policy{ConflictPolicy::KeepLocal};

    RSync() = default;
    ~RSync() override = default;
    explicit RSync(const pqxx::row& row);

    void rehash_config() override;
};

void to_json(nlohmann::json& j, const RSync& s);
void from_json(const nlohmann::json& j, RSync& s);

std::string to_string(const RSync::Strategy& s);
std::string to_string(const RSync::ConflictPolicy& cp);

RSync::Strategy strategyFromString(const std::string& str);
RSync::ConflictPolicy rsConflictPolicyFromString(const std::string& str);

std::string to_string(const std::shared_ptr<RSync>& sync);

}
