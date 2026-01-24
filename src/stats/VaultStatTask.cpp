#include "stats/VaultStatTask.hpp"
#include "types/stats/VaultStat.hpp"

#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "database/Queries/DirectoryQueries.hpp"

using namespace vh::stats;
using namespace vh::types;

VaultStatTask::VaultStatTask(const unsigned int vaultId) : vaultId(vaultId), stat(std::make_unique<VaultStat>(vaultId)) {}

void VaultStatTask::operator()() {
}
