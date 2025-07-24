#pragma once

#include "SyncTask.hpp"

namespace vh::concurrency {

class MirrorSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~MirrorSyncTask() override = default;

    MirrorSyncTask(const std::shared_ptr<storage::StorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller) : SyncTask(engine, controller) {}

    void sync() override;

private:
    void syncKeepLocal();
    void syncKeepRemote();
};

}
