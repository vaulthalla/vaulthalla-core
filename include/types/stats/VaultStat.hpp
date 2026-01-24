#pragma once
#include <memory>

namespace vh::types {

struct CapacityStats;

struct VaultStat {
    std::unique_ptr<CapacityStats> capacity;

    explicit VaultStat(unsigned int vaultId);
};

}
