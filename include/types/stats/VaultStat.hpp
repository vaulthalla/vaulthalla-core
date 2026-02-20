#pragma once

#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace vh::types {

namespace sync {
struct Event;
}

struct CapacityStats;

struct VaultStat {
    std::shared_ptr<CapacityStats> capacity;
    std::shared_ptr<sync::Event> latest_sync_event;

    explicit VaultStat(unsigned int vaultId);
};

void to_json(nlohmann::json& j, const std::shared_ptr<VaultStat>& s);

}
