#pragma once

#include "SyncTask.hpp"

namespace vh::concurrency {

class SafeSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~SafeSyncTask() override = default;

    SafeSyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine) : SyncTask(engine) {}

    void sync() override;
};

}
