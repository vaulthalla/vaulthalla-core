#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>

namespace vh::services {
class SyncController;
}

namespace vh::storage {
struct CloudStorageEngine;
}

namespace vh::types {
struct File;
}

namespace vh::cloud {

class SyncTask : public concurrency::Task, public std::enable_shared_from_this<SyncTask> {
public:
    std::chrono::system_clock::time_point next_run;

    explicit SyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
                      const std::shared_ptr<services::SyncController>& syncController);

    ExpectedFuture operator()() override;

    bool operator<(const SyncTask& other) const;

private:
    static constexpr std::string CONTENT_HASH_ID = "x-amz-meta-content-hash";
    std::shared_ptr<storage::CloudStorageEngine> engine_;
    std::shared_ptr<services::SyncController> controller_;

    void sync(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const;

    void downloadDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const;
};

}
