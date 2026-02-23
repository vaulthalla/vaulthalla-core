#pragma once

#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace vh::storage::stats { struct Capacity; }

namespace vh::types {

struct VaultStat {
    uint32_t vault_id;
    std::shared_ptr<storage::stats::Capacity> capacity;

    explicit VaultStat(unsigned int vaultId);
};

void to_json(nlohmann::json& j, const std::shared_ptr<VaultStat>& s);

}
