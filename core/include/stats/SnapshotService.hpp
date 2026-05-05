#pragma once

#include "concurrency/AsyncService.hpp"

#include <chrono>
#include <cstdint>

namespace vh::stats {

class SnapshotService final : public concurrency::AsyncService {
public:
    SnapshotService();

protected:
    void runLoop() override;

private:
    void collectRuntimeSnapshots();
    void collectVaultSnapshots();
    void purgeOldSnapshots();

    std::chrono::steady_clock::time_point lastRuntimeCollection_{};
    std::chrono::steady_clock::time_point lastVaultCollection_{};
    std::chrono::steady_clock::time_point lastPurge_{};
};

}
