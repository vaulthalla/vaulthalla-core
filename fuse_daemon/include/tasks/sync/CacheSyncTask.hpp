#pragma once

#include "SyncTask.hpp"

#include <utility>

namespace vh::types {
    struct CacheIndex;
}

namespace vh::concurrency {

class CacheSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~CacheSyncTask() override = default;

    CacheSyncTask(const std::shared_ptr<storage::StorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller) : SyncTask(engine, controller) {}

    void sync() override;

private:
    static std::pair<uintmax_t, uintmax_t> computeIndicesSizeAndMaxSize(const std::vector<std::shared_ptr<types::CacheIndex>>& indices);

    void ensureFreeSpace(uintmax_t size) const override;
};


}
