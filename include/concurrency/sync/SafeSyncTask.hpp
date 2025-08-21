#pragma once

#include "SyncTask.hpp"

namespace vh::concurrency {

class SafeSyncTask final : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~SafeSyncTask() override = default;

    explicit SafeSyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine);

    void sync() override;
};

}
