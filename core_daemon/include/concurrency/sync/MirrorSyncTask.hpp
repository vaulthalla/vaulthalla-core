#pragma once

#include "concurrency/sync/SyncTask.hpp"

namespace vh::concurrency {

class MirrorSyncTask : public SyncTask {
public:
    using SyncTask::SyncTask;

    ~MirrorSyncTask() override = default;

    void sync(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const override;
    void handleDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const override;

private:
    void syncKeepLocal(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const;
    void syncKeepRemote(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const;
};

}
