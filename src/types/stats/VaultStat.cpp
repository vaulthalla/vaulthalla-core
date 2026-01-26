#include "types/stats/VaultStat.hpp"
#include "types/stats/CapacityStats.hpp"

#include <nlohmann/json.hpp>

using namespace vh::types;

VaultStat::VaultStat(unsigned int vaultId) : capacity(std::make_shared<CapacityStats>(vaultId)) {}

void vh::types::to_json(nlohmann::json& j, const std::shared_ptr<VaultStat>& s) {
    j["capacity"] = s->capacity;
}
