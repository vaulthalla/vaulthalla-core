#pragma once

#include "SyncTask.hpp"

namespace vh::concurrency {

class MirrorSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~MirrorSyncTask() override = default;

    MirrorSyncTask(const std::shared_ptr<storage::StorageEngine>& engine) : SyncTask(engine) {}

    void sync() override;

private:
    void syncKeepLocal();
    void syncKeepRemote();
};

}
