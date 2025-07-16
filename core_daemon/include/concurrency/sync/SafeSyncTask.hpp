#pragma once

#include "concurrency/sync/SyncTask.hpp"

namespace vh::concurrency {

class SafeSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~SafeSyncTask() override = default;

    SafeSyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller) : SyncTask(engine, controller) {}

    void sync(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const override;
    void handleDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const override;
};

}
