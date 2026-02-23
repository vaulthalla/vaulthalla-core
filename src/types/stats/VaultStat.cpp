#include "types/stats/VaultStat.hpp"
#include "types/stats/CapacityStats.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "sync/model/Event.hpp"

#include <nlohmann/json.hpp>

using namespace vh::types;
using namespace vh::services;

VaultStat::VaultStat(unsigned int vaultId)
    : vault_id(vaultId),
      capacity(std::make_shared<CapacityStats>(vaultId)) {}

void vh::types::to_json(nlohmann::json& j, const std::shared_ptr<VaultStat>& s) {
    j["capacity"] = s->capacity;
    j["latest_sync_event"] = ServiceDepsRegistry::instance().storageManager->getEngine(s->vault_id)->latestSyncEvent;
}
