#pragma once

#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace vh::types {

struct CapacityStats;

struct VaultStat {
    std::shared_ptr<CapacityStats> capacity;

    explicit VaultStat(unsigned int vaultId);
};

void to_json(nlohmann::json& j, const std::shared_ptr<VaultStat>& s);

}
