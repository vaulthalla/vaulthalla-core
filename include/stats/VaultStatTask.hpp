#pragma once

#include "concurrency/Task.hpp"

namespace vh::types {
struct VaultStat;
}

namespace vh::stats {

struct VaultStatTask final : concurrency::PromisedTask {
    unsigned int vaultId;
    std::shared_ptr<types::VaultStat> stat;

    explicit VaultStatTask(unsigned int vaultId);
    ~VaultStatTask() override = default;

    void operator()() override;
};

}
