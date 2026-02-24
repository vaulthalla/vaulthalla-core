#pragma once

#include "concurrency/Task.hpp"

namespace vh::vault::model { struct Stat; }

namespace vh::vault::task {

struct Stats final : concurrency::PromisedTask {
    unsigned int vaultId;
    std::shared_ptr<model::Stat> stat;

    explicit Stats(unsigned int vaultId);
    ~Stats() override = default;

    void operator()() override;
};

}
