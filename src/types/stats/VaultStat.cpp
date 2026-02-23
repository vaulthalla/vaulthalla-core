#include "types/stats/VaultStat.hpp"
#include "storage/model/stats/Capacity.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/Manager.hpp"
#include "storage/Engine.hpp"
#include "sync/model/Event.hpp"

#include <nlohmann/json.hpp>

using namespace vh::types;
using namespace vh::services;

VaultStat::VaultStat(unsigned int vaultId)
    : vault_id(vaultId),
      capacity(std::make_shared<storage::stats::Capacity>(vaultId)) {}

void vh::types::to_json(nlohmann::json& j, const std::shared_ptr<VaultStat>& s) {
    j["capacity"] = s->capacity;
    j["latest_sync_event"] = ServiceDepsRegistry::instance().storageManager->getEngine(s->vault_id)->latestSyncEvent;
}
