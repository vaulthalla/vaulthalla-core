#pragma once

#include "concurrency/sync/SyncTask.hpp"

namespace vh::types {
    struct CacheIndex;
}

namespace vh::concurrency {

class CacheSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~CacheSyncTask() override = default;

    CacheSyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller) : SyncTask(engine, controller) {}

    void sync(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const override;
    void handleDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const override;

private:
    static uintmax_t sumIndicesSize(const std::vector<std::shared_ptr<types::CacheIndex>>& indices);

    [[nodiscard]] bool shouldPurgeNewFiles() const;
};


}
