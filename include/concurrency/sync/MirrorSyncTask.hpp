#pragma once

#include "SyncTask.hpp"

namespace vh::concurrency {

class MirrorSyncTask final : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~MirrorSyncTask() override = default;

    MirrorSyncTask(const std::shared_ptr<storage::StorageEngine>& engine);

    void sync() override;

private:
    void syncKeepLocal();
    void syncKeepRemote();
};

}
