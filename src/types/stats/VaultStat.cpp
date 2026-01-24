#include "types/stats/VaultStat.hpp"
#include "types/stats/CapacityStats.hpp"

using namespace vh::types;

VaultStat::VaultStat(unsigned int vaultId) : capacity(std::make_unique<CapacityStats>(vaultId)) {}
