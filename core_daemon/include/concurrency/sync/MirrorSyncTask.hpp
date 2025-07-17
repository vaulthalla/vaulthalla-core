#pragma once

#include "concurrency/sync/SyncTask.hpp"

namespace vh::concurrency {

class MirrorSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~MirrorSyncTask() override = default;

    void sync();

private:
    void syncKeepLocal();
    void syncKeepRemote();
};

}
